```c
// 物理内存分配器，用于用户进程、内核堆栈、页表页面、和管道缓冲区
// 分配整个 4096 字节页面。

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"


// 释放从pa_start到pa_end之间的内存
void freerange(void *pa_start, void *pa_end);

extern char end[]; // 表示内核结束后的第一个地址
                   // 这个地址在链接脚本kernel.ld中定义


//用于表示空闲内存块。它包含一个指向下一个空闲内存块的指针next
struct run {
  struct run *next;
};


// 用于管理空闲内存。它包含一个自旋锁lock和一个指向空闲内存块链表的指针freelist
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;


// 内存初始化函数
void
kinit()
{
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

// 释放 v 指向的物理内存页面，该页面通常应由对 kalloc() 的调用返回
// 例外情况是当初始化分配器时；请参阅上面的 kinit
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // 用垃圾填充以捕获悬垂引用
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// 分配一个 4096 字节的物理内存页 
// 返回内核可以使用的指针
// 如果无法分配内存，则返回 0
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
```