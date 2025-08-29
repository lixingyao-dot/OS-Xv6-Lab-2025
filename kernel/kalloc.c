// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
struct spinlock ref_lock;
uint8 ref_count[PHYSTOP / PGSIZE];

// 初始化引用计数数组和锁
void
ref_init()
{
  initlock(&ref_lock, "ref_count");
  for (int i = 0; i < PHYSTOP / PGSIZE; i++) {
    ref_count[i] = 0;
  }
}

// 增加物理地址pa的引用计数
void
incref(uint64 pa)
{
  acquire(&ref_lock);
  if(pa >= PHYSTOP) {
    panic("incref: pa out of range");
  }
  int index = pa / PGSIZE;
  if(ref_count[index] < 255) {
    ref_count[index]++;
  } else {
    panic("incref: ref_count overflow");
  }
  release(&ref_lock);
}

// 减少物理地址pa的引用计数，返回是否计数为0
int
decref(uint64 pa)
{
  acquire(&ref_lock);
  if(pa >= PHYSTOP) {
    panic("decref: pa out of range");
  }
  int index = pa / PGSIZE;
  if(ref_count[index] == 0) {
    // 页面没有被引用，直接返回1表示可以释放
    release(&ref_lock);
    return 1;
  }
  ref_count[index]--;
  int ret = ref_count[index] == 0;
  release(&ref_lock);
  return ret;
}
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
  ref_init(); // 初始化引用计数
  initlock(&kmem.lock, "kmem");
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

  // 减少引用计数，如果计数不为0，则返回
  if(decref((uint64)pa) == 0) {
    return;
  }

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

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    incref((uint64)r); // 设置引用计数为1
  }
  return (void*)r;
}
