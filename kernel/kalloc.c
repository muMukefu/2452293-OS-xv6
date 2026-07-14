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

//my alter: 定义超级页区域的起始地址,预留32个超级页
#define SUPERBASE (PHYSTOP - 32 * SUPERPGSIZE)

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
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)SUPERBASE);  //my alter: SUPERBASE
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
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

//my alter
struct superpage_run {
  struct superpage_run* next;
};

struct {
  struct spinlock lock;
  struct superpage_run* freelist;
} superpage_kmem;

void
superpage_init(void)
{
  initlock(&superpage_kmem.lock, "superpage_kmem");

  //从SUPERBASE开始，PHYSTOP结束
  uint64 base = SUPERBASE;
  //确保2MB对齐
  if (base % SUPERPGSIZE != 0) {
    base = ((base / SUPERPGSIZE) + 1) * SUPERPGSIZE;
  }

  for (uint64 pa = base; pa + SUPERPGSIZE <= PHYSTOP; pa += SUPERPGSIZE) {
    struct superpage_run* r = (struct superpage_run*)pa;
    r->next = superpage_kmem.freelist;
    superpage_kmem.freelist = r;
  }
}

//分配超级页
void*
superalloc(void)
{
  struct superpage_run* r;
  acquire(&superpage_kmem.lock);

  r = superpage_kmem.freelist;
  if (r)
    superpage_kmem.freelist = r->next;
  release(&superpage_kmem.lock);

  if (r)
    memset((char*)r, 0, SUPERPGSIZE);
  return (void*)r;
}

//释放超级页
void
superfree(void* pa)
{
  struct superpage_run* r;

  if (((uint64)pa % SUPERPGSIZE) != 0 || (uint64)pa >= PHYSTOP)
    panic("superfree");

  r = (struct superpage_run*)pa;

  acquire(&superpage_kmem.lock);
  r->next = superpage_kmem.freelist;
  superpage_kmem.freelist = r;
  release(&superpage_kmem.lock);
}
