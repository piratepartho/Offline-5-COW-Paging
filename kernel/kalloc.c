// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#include <inttypes.h>

#define STATUSSIZE 1000

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct pageStatus{
  uint pid;
  uint64 pa;
  struct pageStatus *next;
};

struct{
  struct spinlock lock;
  struct pageStatus *freeList;
  struct pageStatus *liveList;
} pages;




void initLivePage(){
  
  acquire(&pages.lock);
  pages.freeList = 0;
  pages.liveList = 0;

  for(int i = 0; i < STATUSSIZE; i++)
  {
    char* start;
    // printf("Here1\n");
    if((start = kalloc()) == 0){
      panic("initLivePage(): bad kalloc");
    }
    for(uint64 i = (uint64) start; i+sizeof(struct pageStatus) < (uint64)start + PGSIZE; i += sizeof(struct pageStatus)){ // i is the physical address
      struct pageStatus *s;
      s = (struct pageStatus*) i; // make the pa pageStatus pointer
      s->next = pages.freeList;
      pages.freeList = s;
    }
  }

  // printf("lock acquired"); 
  

  
  release(&pages.lock);
}

void addToLivePage(uint64 pa){
  acquire(&pages.lock);

  struct pageStatus* pg = pages.freeList;
  if(pg == 0) {
    // sys_getLivePage();
    panic("addToLivePage() : no free pages");
  }
  pages.freeList = pg->next;

  pg->pid = myproc()->pid;
  pg->pa = pa;

  if(pages.liveList == 0){
    pages.liveList = pg;
    pg->next = 0;
    release(&pages.lock);
    return;
  }

  struct pageStatus* i;
  for(i = pages.liveList; i->next != 0 ; i = i->next) ; // go to the last livepage
  i->next = pg; // add the pageStatus to liveList
  pg->next = 0; // at the end added

  release(&pages.lock);
}

void addToFreeList(struct pageStatus* pg){
  // assumed lock help by caller
  pg->pid = -1;
  pg->pa = -1;

  pg->next = pages.liveList;
  pages.liveList = pg;
}

void removeFromLivePage(uint64 pa){
  acquire(&pages.lock);

  struct pageStatus* pg = pages.liveList;
  struct pageStatus* prev = 0;
  
  while(1){
    if(pg == 0){
      printf("rmLivePage() : no %d in Live\n", pa);
      release(&pages.lock);
      break;
    }

    // printf("here %d %d\n", pg->pid, pg->pa);

    if(pg->pa == pa){
      if(prev != 0) prev->next = pg->next;
      else pages.liveList = pg->next;
      addToFreeList(pg);
      release(&pages.lock);
      break;
    }

    prev = pg;
    pg = pg->next;
  
  }
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*) PHYSTOP); 
  initLivePage();
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void 
ukfree(void *pa)
{
  removeFromLivePage((uint64) pa);
  kfree(pa);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
  {
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r; 
}

void *
ukalloc(void){
  char* r = kalloc();
  
  addToLivePage((uint64)r);

  return (void*) r;
}


uint64
sys_getLivePage(void)
{
  acquire(&pages.lock);
  
  int cnt[NPROC]; //! should fail when pid is greater than NPROC, ignoring special case like this
  
  for(int n = 0; n < NPROC; n++){
    cnt[n] = 0;
  }

  struct pageStatus *i;
  for(i = pages.liveList; i != 0; i = i->next){
    cnt[ i->pid ]++; 
  }
  
  for(int n = 0; n < NPROC; n++){
    if(cnt[n] > 0) printf("pid: %d, live pages: %d\n",n,cnt[n]);
  }
  
  release(&pages.lock);
  
  return 0;
}