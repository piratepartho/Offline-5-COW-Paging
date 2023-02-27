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
#define MAXPHYPAGES 50

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
  pagetable_t pt;
  uint64 va;
  struct pageStatus *next;
};

struct{
  struct spinlock lock;
  struct pageStatus *freeList;
  struct pageStatus *liveList;
  int liveCnt;
} pages;

int livepageswap = 1;
void initLivePage(){
  
  acquire(&pages.lock);
  pages.freeList = 0;
  pages.liveList = 0;
  pages.liveCnt = 0;

  for(int i = 0; i < STATUSSIZE; i++)
  {
    char* start;
    
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

  release(&pages.lock);
}

void addToLivePage(pagetable_t pt, uint64 va){
  acquire(&pages.lock);

  struct pageStatus* pg = pages.freeList;
  if(pg == 0) {
    // sys_getLivePage();
    panic("addToLivePage() : no free pages");
  }
  pages.freeList = pg->next;

  pg->pid = myproc()->pid;
  pg->pt = pt;
  pg->va = va;

  if(pages.liveList == 0){
    pages.liveList = pg;
    pg->next = 0;
    pages.liveCnt++;
    release(&pages.lock);
    return;
  }

  struct pageStatus* i;
  for(i = pages.liveList; i->next != 0 ; i = i->next) ; // go to the last livepage
  i->next = pg; // add the pageStatus to liveList
  pg->next = 0; // at the end added
  pages.liveCnt++;

  if(pages.liveCnt >= 30 && livepageswap){
    livepageswap = 0;
    pagetable_t rempt = pg->pt;
    uint64 remva = pg->va;
    uint64 pa = PTE2PA(*walk(rempt, remva, 0));
    printf("%d %d %d\n", rempt, remva, pa);

    struct swap* sp = swapalloc();
    printf("swapping cnt: %d\n",pages.liveCnt);
    release(&pages.lock);
    
    // swapout(sp, (char*) pa);
    
    removeFromLivePage(pt, va);
    sys_getLivePage();
    // swapin((char *) pa,sp);
    
    addToLivePage(rempt, remva);
    sys_getLivePage();
    
    swapfree(sp);
    
    acquire(&pages.lock);
  }

  release(&pages.lock);
}

void addToFreeList(struct pageStatus* pg){
  // assumed lock help by caller
  pg->pid = -1;
  pg->pt = (pagetable_t) -1;
  pg->va = -1;

  pg->next = pages.freeList;
  pages.freeList = pg;
  pages.liveCnt--;
}

void removeFromLivePage(pagetable_t pt, uint64 va){
  acquire(&pages.lock);

  struct pageStatus* pg = pages.liveList;
  struct pageStatus* prev = 0;
  
  while(1){
    if(pg == 0){
      //! gives an error initially that case error has been skipped
      printf("oh no\n");
      release(&pages.lock);
      break;
    }

    // printf("here %d %d\n", pg->pid, pg->pa);

    if(pg->va == va && pg->pt == pt){
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

// void 
// ukfree(pagetable_t pt, uint64 va, void *pa)
// {
//   removeFromLivePage(pt, va);
//   kfree(pa);
// }

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

// void *
// ukalloc(pagetable_t pt, uint64 va){
//   char* r = kalloc();
  
//   addToLivePage(pt, va);

//   return (void*) r;
// }


uint64
sys_getLivePage(void)
{
  acquire(&pages.lock);
  
  int cnt[NPROC]; //! should fail when pid is greater than NPROC, ignoring special case like this
  
  for(int n = 0; n < NPROC; n++){
    cnt[n] = 0;
  }
  // printf("here1");
  struct pageStatus *i;
  for(i = pages.liveList; i != 0; i = i->next){
    // printf("pid: %d",i->pid);
    if( i->pid >= NPROC ){
      printf("!!! pid > NPROC, per proc live page will show error. Total count ok. !!!\n");
      break;
    }
    cnt[ i->pid ]++; 
  }
  // printf("here2");
  for(int n = 0; n < NPROC; n++){
    if(cnt[n] > 0) printf("pid: %d, live pages: %d\n",n,cnt[n]);
  }
  
  printf("total live pages: %d\n ", pages.liveCnt);
  release(&pages.lock);
  
  return 0;
}