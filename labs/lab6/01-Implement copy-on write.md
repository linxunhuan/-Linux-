# Copy-on-Write Fork for xv6
+ 虚拟内存提供了一定程度的间接寻址：
  + 内核可以通过将PTE标记为无效或只读来拦截内存引用，从而导致页面错误，还可以通过修改PTE来更改地址的含义
  + 在计算机系统中有一种说法，任何系统问题都可以用某种程度的抽象方法来解决
  + Lazy allocation实验中提供了一个例子
  + 这个实验探索了另一个例子：写时复制分支（copy-on write fork）

## 问题：
+ xv6中的fork()系统调用将父进程的所有用户空间内存复制到子进程中
+ 如果父进程较大，则复制可能需要很长时间
+ 更糟糕的是，这项工作经常造成大量浪费
  + 例如，子进程中的fork()后跟exec()将导致子进程丢弃复制的内存
  + 而其中的大部分可能都从未使用过
+ 另一方面，如果父子进程都使用一个页面，并且其中一个或两个对该页面有写操作，则确实需要复制

## 解决方案
+ copy-on-write (COW) fork()的目标是
  + 推迟到子进程实际需要物理内存拷贝时再进行分配和复制物理内存页面
+ COW fork()只为子进程创建一个页表，用户内存的PTE指向父进程的物理页
  + COW fork()将父进程和子进程中的所有用户PTE标记为不可写
  + 当任一进程试图写入其中一个COW页时，CPU将强制产生页面错误
+ 内核页面错误处理程序检测到这种情况
  + 将为出错进程分配一页物理内存
  + 将原始页复制到新页中
  + 修改出错进程中的相关PTE指向新的页面
  + 将PTE标记为可写
+ 当页面错误处理程序返回时，用户进程将能够写入其页面副本
+ COW fork()将使得释放用户内存的物理页面变得更加棘手
  + 给定的物理页可能会被多个进程的页表引用，并且只有在最后一个引用消失时才应该被释放

## Implement copy-on write
+ 您的任务是在xv6内核中实现copy-on-write fork
+ 如果修改后的内核同时成功执行cowtest和usertests程序就完成了
+ 为了帮助测试你的实现方案，我们提供了一个名为cowtest的xv6程序（源代码位于user/cowtest.c）
  + cowtest运行各种测试，但在未修改的xv6上，即使是第一个测试也会失败
  + 因此，最初您将看到：
```shell
$ cowtest
simple: fork() failed
$ 
```
+ “simple”测试分配超过一半的可用物理内存，然后执行一系列的fork()
+ fork失败的原因是没有足够的可用物理内存来为子进程提供父进程内存的完整副本
+ 完成本实验后，内核应该通过cowtest和usertests中的所有测试。即：
```shell
$ cowtest
simple: ok
simple: ok
three: zombie!
ok
three: zombie!
ok
three: zombie!
ok
file: ok
ALL COW TESTS PASSED
$ usertests
...
ALL TESTS PASSED
$
```
+ 这是一个合理的攻克计划：
  + 修改uvmcopy()将父进程的物理页映射到子进程，而不是分配新页
    + 在子进程和父进程的PTE中清除PTE_W标志
  + 修改usertrap()以识别页面错误
    + 当COW页面出现页面错误时，使用kalloc()分配一个新页面
    + 并将旧页面复制到新页面
    + 然后将新页面添加到PTE中并设置PTE_W
  + 确保每个物理页在最后一个PTE对它的引用撤销时被释放——而不是在此之前
    + 这样做的一个好处是——为每个物理页保留引用该页面的用户页表数的“引用计数”
      + 当kalloc()分配页时，将页的引用计数设置为1
      + 当fork导致子进程共享页面时，增加页的引用计数
      + 每当任何进程从其页表中删除页面时，减少页的引用计数
      + kfree()只应在引用计数为零时将页面放回空闲列表
      + 可以将这些计数保存在一个固定大小的整型数组中
      + 你必须制定一个如何索引数组以及如何选择数组大小的方案
        + 例如，您可以用页的物理地址除以4096对数组进行索引
        + 并为数组提供等同于kalloc.c中kinit()在空闲列表中放置的所有页面的最高物理地址的元素数
  + 修改copyout()在遇到COW页面时使用与页面错误相同的方案

## 官方提示
+ lazy page allocation实验可能已经让您熟悉了许多与copy-on-write相关的xv6内核代码
  + 但是，您不应该将这个实验建立在您的lazy allocation解决方案的基础上
  + 相反，请按照上面的说明从一个新的xv6开始
+ 有一种可能很有用的方法来记录每个PTE是否是COW映射
  + 您可以使用RISC-V PTE中的RSW（reserved for software，即为软件保留的）位来实现此目的
+ usertests检查cowtest不测试的场景，所以别忘两个测试都需要完全通过
+ kernel/riscv.h的末尾有一些有用的宏和页表标志位的定义
+ 如果出现COW页面错误并且没有可用内存，则应终止进程

## 解题思路
### 第一步：设置标记位
+ 在risc.h中设置
```c
#define PTE_COW (1L << 9) // 记录应用了COW策略后fork的页面
```
### 第二步：修改 uvmcopy()
+ 修改成：不要立即分配子进程的内存（把映射map到物理内存）
+ 而是只修改PTE_W和PTE_COW
  + 父、子进程均不可写——PTE_W
  + PTE_COW将这一页改为COW页（子进程）
+ 修改如下
```c
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;        // 存储页表项的指针
  uint64 pa, i;      // pa是物理地址，i是循环变量
  uint flags;        // 页表项的标志位
  

  for(i = 0; i < sz; i += PGSIZE){

    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist"); 
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");  

    // 获取该页表项对应的物理地址
    pa = PTE2PA(*pte);
    
    // 获取页表项的标志位（包括访问权限等）
    flags = PTE_FLAGS(*pte);
    
    // 不分配内存，而是设置标记
    if (flags & PTE_W){
      flags = (flags | PTE_COW) & (~PTE_W);
      *pte = PA2PTE(pa) | flags;
    }

    increase_count((void *)pa);// 增加计数

    // 将物理页面映射到新页表（新进程的地址空间）
    if(mappages(new, i, PGSIZE, pa, flags) != 0){
      goto err;
    }
  }

  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1; 
}
```
### 第三步：修改usertrap()以识别页面错误
+ 修改trap.c中的usertrap() 函数
+ 利用trap机制来处理这个缺页中断（类似于实验五）
  + 判断有缺页中断，就先判断一下当前地址是否有效
    + （报错的虚拟地址超过最大的虚拟地址，或者访问的是每个进程的guard page）
  + 然后给有COW标志位的页表进行副本copy
  + 然后设置为可写
```c
 else if (r_scause() == 13 ||r_scause() == 15) {
    uint64 va = r_stval();

    // 保证当前页地址有效
    if(va >= MAXVA ||
    (va <= PGROUNDDOWN(p ->trapframe->sp)
    && va >= PGROUNDDOWN(p ->trapframe->sp) - PGSIZE)){
      p -> killed = 1;
    }else if(uvmcowcopy(p -> pagetable,va) != 0){
      // 判断页是否是cow页，并且为其实际分配内存
      p -> killed = 1;
    }
  }
```
### 第四步：当COW页面出现页面错误时，使用kalloc()分配一个新页面
+ 这里就是对应上一步里面的uvmcopy()函数的
```c
int
uvmcowcopy(pagetable_t pagetable, uint64 va){
  uint64 pa;  // 物理地址
  pte_t* pte; // 页表项指针
  uint flags; // 页表项标志位

  // 如果虚拟地址超出了最大虚拟地址范围，则返回错误
  if (va >= MAXVA) return -1;

  va = PGROUNDDOWN(va);

  pte = walk(pagetable, va, 0);
  if (pte == 0) return -1;

  pa = PTE2PA(*pte);
  if (pa == 0) return -1;

  flags = PTE_FLAGS(*pte);

  // 判断是不是cow页
  if(flags & PTE_COW){
    char *ka = kalloc();

    // 源物理页数据拷贝到新的内核空间
    memmove(ka, (char*)pa, PGSIZE);

    // 去除 COW 标志并设置为可写（PTE_W）
    flags = (flags & ~PTE_COW) | PTE_W;
    *pte = PA2PTE(pa) | flags;

    kfree((void*)pa);
  }
  return 0;
}
```
### 第五步：设置引用计数
+ 仿照kmem写
+ 题目中的提示：
      + 可以将这些计数保存在一个固定大小的整型数组中
      + 你必须制定一个如何索引数组以及如何选择数组大小的方案
        + 例如，您可以用页的物理地址除以4096对数组进行索引
+ 在kalloc.c中
```c
// 将一个物理地址（pa）转换为一个索引值
// 将地址右移12位，相当于将地址除以页面大小（PGSIZE），因为 PGSIZE 为 4KB，即 2^12 字节
#define PA2IDX(pa) (((uint64)pa) >> 12)

struct
{
  struct spinlock lock;                    // 一个自旋锁，确保在多核处理器上访问引用计数时的同步
  int count[PGROUNDUP(PHYSTOP) / PGSIZE];  // 用于存储每个页面的引用计数
} mem_ref;

void
increase_rc(void *pa)
{
  acquire(&mem_ref.lock);       // 获取锁
  mem_ref.count[PA2IDX(pa)]++;  // 将对应页面的引用计数加1
  release(&mem_ref.lock);       // 释放锁
}

void
decrease_rc(void *pa)
{
  acquire(&mem_ref.lock);       // 获取锁
  mem_ref.count[PA2IDX(pa)]--;  // 将对应页面的引用计数减1
  release(&mem_ref.lock);       // 释放锁
}

int 
get_rc(void *pa){
  acquire(&mem_ref.lock);              // 获取锁
  int rc = mem_ref.count[PA2IDX(pa)];  // 获取对应页面的引用计数
  release(&mem_ref.lock);              // 释放锁
  return rc;  
}

void
kinit()
{
  initlock(&mem_ref.lock,"mem_ref");  // 初始化锁
  acquire(&kmem.lock);                // 获取内存管理锁

  // 将计数都初始化为0
  for(int i=0; i < PGROUNDUP(PHYSTOP)/ PGSIZE; i++)
    mem_ref.count[i] = 0; // 初始化所有页面引用计数为0
  
  release(&kmem.lock);  // 释放内存管理锁  
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}
```
### 第六步：设置kfree()函数
+ 在释放页面时，需要考虑这个界面的使用计数是否为0
+ 每次访问kfree时，就将这个计数减一，如果为0，就直接彻底free掉就行
```c
void
kfree(void *pa){
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  decrease_rc(pa);
  if (get_rc(pa) > 0)
    return;
  
   // 如果引用计数已经是 0，说明没有其他地方再使用这个页面了，准备释放它
  memset(pa, 0, PGSIZE);
  r = (struct run*)pa;
  acquire(&kmem.lock);
  
  // 将释放的页面添加到内存空闲链表中
  r->next = kmem.freelist;  // 将当前页面插入到空闲链表的头部
  kmem.freelist = r;        // 更新空闲链表头指针为新的页面

  release(&kmem.lock);
}
```

### 第七步：每当给COW页分配内存时，就需要增加一个计数
+ 这里是void * ，是因为defs里面声明的，就是void *
```c
void*
kalloc(void){
  struct run *r;

  // 加锁，确保在访问共享资源 kmem.freelist 时的线程安全
  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE);
    increase_rc((void*)r);
  
  return (void*)r;
}
```































```c
void *
kalloc(void)
{
  struct run *r;  

  // 获取 kmem 锁，确保在访问内存池时的同步操作
  acquire(&kmem.lock);  

   // 获取当前空闲内存块链表的头指针（freelist）。此时 r 指向一个空闲内存块
  r = kmem.freelist; 
  
  if(r)
  // 如果 freelist 非空，将 freelist 更新为下一个空闲块，即 r->next
    kmem.freelist = r->next; 
  release(&kmem.lock);  // 释放 kmem 锁，完成对内存池的访问。

  if(r)
    memset((char*)r, 5, PGSIZE); 
// 如果 r 非空，表示成功获取到一块内存。用 `memset` 将这块内存填充为值 5（表示“垃圾”数据）
//（`PGSIZE` 是每页的大小通常是 4KB 或 4096 字节）
  
  if(r) {
    //acquire(&pa_ref_lock);  
     // 对物理页的引用计数加 1
     // 这里通过将指针 r 转换为物理地址并右移 12 位来计算物理页号
     // 假设页大小为 4KB，即 12 位移位。
     pa_ref_count[(uint32)(uint64)r >> 12]++; 
    //release(&pa_ref_lock);  
  }

  return (void*)r;  // 返回指向分配内存块的指针 r。
}
```

































