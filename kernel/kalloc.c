// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define FREE_MAX_CNT PHYSTOP/PGSIZE

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct ref_cnt {
  struct spinlock lock[FREE_MAX_CNT];
  int cnt[FREE_MAX_CNT];
};

struct {
  struct spinlock lock;
  struct run *freelist;
  struct ref_cnt counter;
} kmem;

void
kinit()
{
  int i;

  initlock(&kmem.lock, "kmem");
  for(i = 0; i < FREE_MAX_CNT; i++) {
    initlock(&kmem.counter.lock[i], "");
    kmem.counter.cnt[i] = 1;
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

void
kaddrefcnt(void *pa)
{
  int ref_cnt_idx = (uint64)pa / PGSIZE;

  acquire(&kmem.counter.lock[ref_cnt_idx]);
  kmem.counter.cnt[ref_cnt_idx]++;
  release(&kmem.counter.lock[ref_cnt_idx]);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int ref_cnt_idx;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  ref_cnt_idx = (uint64)pa / PGSIZE;

  acquire(&kmem.counter.lock[ref_cnt_idx]);
  kmem.counter.cnt[ref_cnt_idx]--;
  if(kmem.counter.cnt[ref_cnt_idx] > 0) {
    release(&kmem.counter.lock[ref_cnt_idx]);
    return;
  }
  release(&kmem.counter.lock[ref_cnt_idx]);
  
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
  int ref_cnt_idx;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    ref_cnt_idx = (uint64)((void*)r) / PGSIZE;
    kmem.counter.cnt[ref_cnt_idx] = 1;
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
    
  return (void*)r;
}
