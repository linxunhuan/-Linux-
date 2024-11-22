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
### 第一步：处理sbrk()负参数
+ 该函数用于改变当前进程的数据段大小
  + 负参数，意味着减少内存
  + 正参数，意味着增大内存
```c
uint64
sys_sbrk(void)
{
  int addr; // 存储当前数据段的末尾地址（也即旧的进程大小）
  int n;    // 表示要增加或减少的数据段大小
  struct proc *p = myproc();

  
  if(argint(0, &n) < 0)
    return -1;
  
  // 保存当前数据段末尾地址（即当前的进程大小）。
  addr = p->sz;
  
  // 根据用户传入的大小 n 进行处理：
  if(n > 0){
    // 如果 n 大于 0，则意味着需要扩展数据段
    p->sz += n;
  }else if (n < 0){
    // 如果 n 小于 0，则意味着需要缩小数据段
    p ->sz = uvmdealloc(p->pagetable,p->sz,p->sz+n);
  }
  
  return addr;
}
```
### 第二步：如果page-faults的虚拟内存地址比sbrk()分配的大，则杀掉此进程
```c
else if(r_scause() == 15 || r_scause() == 13) {
    
    // r_stval()返回 RISC-V stval寄存器，其中包含导致页面错误的虚拟地址
    uint64 va = r_stval();

    // * 如果虚拟内存地址高于使用sbrk分配的任何虚拟地址，则终止该进程
    if (va >= p->sz) {  
       p->killed = 1;
     }else if (va < PGROUNDDOWN(p->trapframe->sp)){
      p->killed = 1;
     }else{
      char *mem = kalloc();
      
      // 将分配的物理内存清零，确保新页面的内容初始化为 0
      memset(mem,0, PGSIZE);
      
      // 将虚拟地址映射到物理地址，并更新进程的页表
      // PGROUNDDOWN(stval) 将 stval 向下取整到页面边界（4KB 对齐的地址）
      if(mappages(p->pagetable, PGROUNDDOWN(va),PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U) != 0){
        kfree(mem);
        p -> killed = 1;
      }
     }
}
```
### 第三步：处理fork() parent-to-child内存拷贝
+ 经查看fork()函数，确定，具体执行由uvmcopy()函数解决
+ 需要处理的两个问题（也就是4和5），仔细检查发现，就是源代码自己panic点
+ 那屏蔽点，省心
```c
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue;// panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      continue;// panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```
### 第四步：处理用户堆栈下无效页面的故障




















