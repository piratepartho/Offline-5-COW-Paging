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
  int refCount[PHYSTOP/PGSIZE];
} kmem;

void dbgPrint(char* c){
  if(DEBUG){
    printf("%s\n",c);
  }
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  dbgPrint("kinit done");
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    acquire(&kmem.lock);
    kmem.refCount[(uint64) p / PGSIZE] = 1; // made it one so that kfree makes it 0
    release(&kmem.lock);
    kfree(p);
  }
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

  uint64 cnt;

  acquire(&kmem.lock);
  if(kmem.refCount[(uint64) pa / PGSIZE] <= 0)
    panic("kfree(): refcount <= 0");
  --kmem.refCount[(uint64) pa / PGSIZE];
  cnt = kmem.refCount[(uint64) pa / PGSIZE];
  release(&kmem.lock);

  if(cnt > 0) return; 

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

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    acquire(&kmem.lock);
    if(kmem.refCount[(uint64) r / PGSIZE] != 0)
      panic("kalloc(): refCount != 0");
    kmem.refCount[(uint64) r / PGSIZE] = 1;
    release(&kmem.lock);
  }

  return (void*)r;
}

int getFreeMemorySize()
{
  struct run  *r;
  int cnt=0;
  acquire(&kmem.lock);
  for(r = kmem.freelist; r; r=r->next){
    cnt += PGSIZE;
  }
  release(&kmem.lock);
  return cnt;
}

void increaseRef(uint64 pa){
  acquire(&kmem.lock);
  if(kmem.refCount[(uint64) pa / PGSIZE] == 0)
    panic("increaseRef() : refCount == 0");
  kmem.refCount[(uint64) pa / PGSIZE]++;
  release(&kmem.lock);
}
