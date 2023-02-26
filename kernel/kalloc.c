// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

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
  int pid;
  struct pageStatus *next;
};

struct{
  struct spinlock lock;
  struct pageStatus *freeList;
  struct pageStatus *liveList;
} pages;


void initLivePage(){
  char* start;
  if((start = kalloc()) == 0){
    panic("initLivePage(): bad kalloc");
  }

  acquire(&pages.lock); 
  pages.freeList = 0;
  pages.liveList = 0;

  for(uint64 i = (uint64)start; i < (uint64)start+PGSIZE; i += sizeof(struct pageStatus)){ // i is the physical address
    struct pageStatus *s;
    s = (struct pageStatus*) i; // make the pa pageStatus pointer
    s->next = pages.freeList;
    pages.freeList = s;
  }
  release(&pages.lock);
}

// void addToLivePage(uint64 pa){
  
// }

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  //added one page to kernel
  freerange(end, (void*) PHYSTOP); 
  // freerange(end, (void*)PHYSTOP);
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


uint64
sys_getLivePage(void)
{
  printf("%d\n",sizeof(struct pageStatus));
  acquire(&pages.lock);
  // // printf("end[] address: %d", end);
  // printf("%d %d\n",livePage.tail - livePage.head, livePage.tail);
  struct pageStatus *i;
  int cnt1 = 0, cnt2 = 0;
  for(i = pages.freeList; i != 0 ;){
    cnt1++;
    i = i->next;
  }
  for(i = pages.liveList; i != 0 ;){
    cnt2++;
    i = i->next;
  }
  printf("%d %d\n",cnt1,cnt2);
  release(&pages.lock);
  return 0;
}
