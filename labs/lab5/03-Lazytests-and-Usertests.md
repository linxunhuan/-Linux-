# 惰性测试和用户测试
## 官方介绍
+ 我们为您提供了lazytests，这是一个 xv6 用户程序，用于测试可能给您的惰性内存分配器带来压力的一些特定情况。修改您的内核代码，以便惰性测试 和用户测试均能通过
## 官方提示
1.	处理sbrk()负参数
2.	如果page-faults的虚拟内存地址比sbrk()分配的大，则杀掉此进程
3.	正确处理fork() parent-to-child内存拷贝
4.	处理这么一种情况：进程传递一个来自sbrk()的有效地址给system call例如read or write，但是那些地址的内存尚未分配
5.	正确处理超出内存：如果kalloc()在page fault handler中失败，杀掉当前进程
6.	处理用户堆栈下无效页面的故障

如果你的内核通过了惰性测试和用户测试，那么你的解决方案就是可以接受的：
<img src=".\picture\image3.png">

## 解题步骤

+ 这里要很惭愧的说，经过多个尝试，前面写的那些自以为能绕开报错的，终究还是在这个时候要全部解决掉
+ 这个实验，三个片段其实是一个需求的分三步体现，放在一起看，才能达到需求，所以下面将是若会贯通的解释该如何解决

### 第一步：修改 sys_sbrk,只修改 p->sz 的值而不分配物理内存
```c
uint64
sys_sbrk(void)
{
  int addr;
  int n;

  struct proc* p = myproc();

  if(argint(0, &n) < 0)
    return -1;
  addr = p -> sz;

  // 这是释放内存
  if(n < 0){
    uvmdealloc(p -> pagetable,p ->sz,p -> sz + n);
  }

  // lazy allocation
  p -> sz += n;
  return addr;
}
```
### 第二步：修改 usertrap 用户态 trap 处理函数
+ 如果为缺页异常((r_scause() == 13 || r_scause() == 15))
+ 且发生异常的地址是因为lazy allocation而没有映射的话
  + 就为其分配物理内存，并在页表建立映射：
+ 但在建立页表的时候，需要判断是否符合要求，符合才分配：
  + 处于 [0, p->sz)地址范围之中（进程申请的内存范围）
  + PGROUNDDOWN(va) 将地址向下舍入到页面边界的时候
    + 栈页的低一页故意留成不映射，作为哨兵用于捕捉 stack overflow 错误
  + 该虚拟地址的页表项（PTE）是否有效
+ 既然加函数了，记得去defs.h文件中声明，这里我自己懒了
```c
// usertrap()
 else if (r_scause() == 13 || r_scause() == 15) {
    
    // r_stval()返回 RISC-V stval寄存器，其中包含导致页面错误的虚拟地址
    uint64 va = r_stval();
    if(uvmjudge(va)){
      uvmlazytouch(va);
    }
  }
```
```c
int
uvmjudge(uint64 va){
  
  struct proc *p = myproc();

  // 遍历当前进程的页表，找到与虚拟地址 va 对应的页表项
  pte_t *pte = walk(p ->pagetable, va, 0);

  return va < p ->sz 
    && PGROUNDDOWN(va) != r_sp()
    && (p == 0 || ((*pte & PTE_V) == 0));
}

void
uvmlazytouch(uint64 va){
  struct proc *p = myproc();

  // 分配物理内存页
  char *mem = kalloc();

  if (mem == 0){
    p -> killed = 1;
  }else{

    // 如果分配成功，则用零初始化分配的内存
    memset(mem, 0, PGSIZE);

    // 尝试将分配的物理内存映射到指定的虚拟地址 (va) 
    if(mappages(p -> pagetable, va, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      p -> killed = 1;
    }
  }
}
```
### 第三步：该偷的懒还是要偷的
+ 由于lazy allocation分配的页，在刚分配的时候是没有对应的映射的
  + 所以要把一些原本在遇到无映射地址时会 panic 的函数的行为
    + 直接忽略这样的地址
```c
// 取消虚拟地址映射
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      //panic("uvmunmap: walk");
      continue;
    if((*pte & PTE_V) == 0)
      //panic("uvmunmap: not mapped");
      continue;
    if(PTE_FLAGS(*pte) == PTE_V)
      //panic("uvmunmap: not a leaf");、
      continue;
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}
```
### 第四步：copyin() 和 copyout()函数的修改
+ copyin() 和 copyout()：内核/用户态之间互相拷贝数据
+ 这里会访问到lazy allocation分配但是还没实际分配的页
  + 所以要加一个检测，确保 copy 之前，用户态地址对应的页都有被实际分配和映射
```c
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  if(uvmjudge(dst))
    uvmlazytouch(dst);

  // .......
}
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  if(uvmjudge(dst))
    uvmlazytouch(dst);
  // ....
}
```


















































