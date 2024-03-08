// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

// Get the physical page number of a virtual address
#define PA2PGREF_ID(p) (((p)-KERNBASE)/PGSIZE)

// Get the max virtual address of a physical page number
#define PGREF_MAX_ENTRIES PA2PGREF_ID(PHYSTOP)

struct spinlock pgreflock; // Locks for pageref arrays to prevent memory leaks caused by race conditions.
int pageref[PGREF_MAX_ENTRIES]; // Record the reference count for each physical page

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

void
kinit()
{
  initlock(&kmem.lock, "kmem");、
  initlock(&pgreflock, "pgreflock");
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

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&pgreflock);
  if(--pageref[PA2PGREF_ID(pa)] <= 0){
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&pgreflock);  
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
    PA2PGREF(r) = 1;
  }
  return (void*)r;
}

// allocate physical memory for cow and dereference the original memory
void *
kalloc_n_deref(void* pa)
{
  acquire(&pgreflock);

  if(PA2PAGREF(pa) <= 1){
    release(&pgreflock);
    return pa;
  }

  uint64 newpa = (uint64)kalloc();
  if(newpa == 0){
    release(&pgreflock);
    return 0;
  }
  memmove((char*)newpa, (char*)pa, PGSIZE);

  PA2PAGREF(pa)--;

  release(&pgreflock);
  return (void*)newpa;
}

// Increment the reference count of a physical page
void krefpage(void *pa) {
  acquire(&pgreflock);
  PA2PGREF(pa)++;
  release(&pgreflock);
}