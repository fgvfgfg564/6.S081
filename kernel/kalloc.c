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
  int count;
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, "kmem");
    kmem[i].freelist = 0;
    kmem[i].count = 0;
  }
  freerange(end, (void *)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  push_off();
  struct run *r;
  uint id;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;
  id = cpuid();

  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  kmem[id].count ++;
  release(&kmem[id].lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r, *s;
  uint id;

  push_off();
  id = cpuid();


  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if (r){
    kmem[id].freelist = r->next;
    kmem[id].count --;
  }
  release(&kmem[id].lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  else {
    for (int i = 0; i < NCPU; i++) acquire(&kmem[i].lock);
    int maxi = 0, ind = -1;
    for (int i = 0; i < NCPU; i++) {
      if (kmem[i].count > maxi) {
        maxi = kmem[i].count;
        ind = i;
      }
    }
    if (ind == -1) {
      for (int i = 0; i < NCPU; i++) release(&kmem[i].lock);
      pop_off();
      return 0;
    }

    if (maxi == 1) {
      r = kmem[ind].freelist;
      kmem[ind].freelist = kmem[id].freelist = 0;
      kmem[ind].count = kmem[id].count = 0;
    } else {
      s = kmem[ind].freelist;
      for (int i = 1; i <= maxi / 2 - 1; i++) {
        if (s == 0) {
          printf("i=%d, count=%d\n", i, maxi);
          panic("kalloc");
        }
        s = s->next;
      }
      r = s->next;
      s->next = 0;
      kmem[id].count = (maxi - 1) / 2;
      kmem[id].freelist = r->next;
      kmem[ind].count = maxi / 2;
    }
    for (int i = 0; i < NCPU; i++) release(&kmem[i].lock);
    memset((char *)r, 5, PGSIZE);
  }
  pop_off();
  return (void *)r;
}
