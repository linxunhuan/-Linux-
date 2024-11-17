## 题目：
+ xv6 有一个内核页表，在内核中执行时会用到
+ 内核页表直接映射到物理地址，因此内核虚拟地址 x 映射到物理地址 x
+ xv6 还为每个进程的用户地址空间提供了一个单独的页表，仅包含该进程的用户内存的映射，从虚拟地址零开始
+ 由于内核页表不包含这些映射，因此用户地址在内核中无效
  + 因此，当内核需要使用在系统调用中传递的用户指针（例如，传递给 write() 的缓冲区指针）时，内核必须首先将该指针转换为物理地址
+ 本节和下一节的目标是允许内核直接取消引用用户指针
------------
+ 您的第一项工作是修改内核，以便每个进程在内核中执行时都使用自己的内核页表副本
+ 修改struct proc以维护每个进程的内核页表，并修改调度程序以在切换进程时切换内核页表
+ 对于此步骤，每个进程的内核页表应与现有的全局内核页表相同
+ 如果用户测试运行正常， 则您通过了实验的这一部分

## 官方提示
+ 为进程的内核页表向 struct proc 添加一个字段
+ 为新进程生成内核页表的合理方法是实现修改版的 kvminit，以创建新的页表
  + 而不是修改 kernel_pagetable
  + 您需要从 allocproc 调用此函数
  + 确保每个进程的内核页表都具有该进程内核堆栈的映射
+ 在未修改的 xv6 中，所有内核堆栈都在 procinit 中设置
  + 您需要将部分或全部功能移至 allocproc
+ 修改 scheduler() 以将进程的内核页表加载到核心的 satp 寄存器中
  + （请参阅 kvminithart 以获得灵感）
  + 调用 w_satp() 后，不要忘记调用 sfence_vma()
+ scheduler() 应该在没有进程运行时使用 kernel_pagetable
+ 在 freeproc 中释放进程的内核页表
+ 您需要一种方法来释放页表，而无需释放叶物理内存页
+ vmprint 可能有助于调试页表
+ 可以修改 xv6 函数或添加新函数
  + 您可能至少需要在 kernel/vm.c 和 kernel/proc.c 中执行此操作
  + （但不要修改 kernel/vmcopyin.c、kernel/stats.c、user/usertests.c 和 user/stats.c）
+ 缺少页表映射可能会导致内核遇到页面错误
  + 它将打印一个包含 sepc=0x00000000XXXXXXXX 的错误
  + 您可以通过在 kernel/kernel.asm 中搜索 XXXXXXXX 来找出错误发生的位置

## 做题步骤
### 题目理解
+ 任务二主要是理解内核态页表
+ 由于进程在进入内核态后，页表会自动切换成内核态的页表
  + 如果在内核态接收到进程的虚拟地址，还需要先在用户态将地址转为内核态能识别的物理地址
  + 这样操作很麻烦，可不可以在内核态直接翻译进程的虚拟地址呢
+ 为此，这个任务的目的是
  + 为每个进程新增一个内核态的页表
  + 然后在该进程进入到内核态时，不使用公用的内核态页表，而是使用进程的内核态页表
  + 这样就可以实现在内核态直接使用虚拟地址的功能了。

### 第一步：向 struct proc 添加一个字段
+ 在kernel/proc.h中，结构体proc中添加一个变量
  + 用来存储进程专享的内核态页表
<img src=".\picture\image11.png">

### 第二步：修改kvminit函数

+ 参考kvminit()函数进行修改
```c
void ukvmmap(pagetable_t kpagetable, uint64 va,uint64 pa,uint64 sz,int perm){
  if(mappages(kpagetable, va, sz, pa, perm) != 0)
    panic("ukvmmap");
}

pagetable_t
ukvminit()
{
  pagetable_t kpagetable = (pagetable_t) kalloc();
  memset(kpagetable, 0, PGSIZE);

  // 串口寄存器
  ukvmmap(kpagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio 磁盘接口
  ukvmmap(kpagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  ukvmmap(kpagetable, CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  ukvmmap(kpagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // 将内核文本映射为可执行和只读
  ukvmmap(kpagetable, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // 映射内核数据和我们将要使用的物理 RAM
  ukvmmap(kpagetable, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // 将陷阱入口/出口的蹦床映射到 
  // 内核中的最高虚拟地址
  ukvmmap(kpagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
  return kpagetable; 
}
```
### 第三步：在defs.h中加入声明
<img src=".\picture\image12.png">


### 第四步：从 allocproc 调用此函数
在 kernel/proc.c 中的 allocproc 函数里添加调用:
```c
  // 一个空的用户列表.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // 下面是新加的
  // 一个空的用户内核列表
  p->kpagetable = ukvmint();
  if(p->kpagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
```

### 第五步：将 procinit部分或全部功能移至 allocproc
+ 内核栈的初始化原来是在 kernel/proc.c 中的 procinit 函数内
```c
// 在启动时初始化 proc 表.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");

// 为进程的内核堆栈分配一个页面
// 将其映射到内存的高位，后跟一个无效的保护页面.
      char *pa = kalloc();
      if(pa == 0)
        panic("kalloc");
      uint64 va = KSTACK((int) (p - proc));
      kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
      p->kstack = va;
  }
  kvminithart();
}
```
+ 这部分要求将函数内的代码转移到 allocproc 函数内
+ 因此在上一步初始化内核态页表的代码下面接着添加初始化内核栈的代码：
```c
// 初始化内核栈
  char *pa = kalloc();
  if(pa == 0){
    panic("kalloc");
  }
  uint64 va = KSTACK((int) (p - proc));
  ukvmmap(p->kpagetable, va, sizeof(uint64), PGSIZE,PTE_R | PTE_W);
  p->kstack = va;
```
### 第六步：修改 scheduler(),将进程的内核页表加载到核心的 satp 寄存器中
+ 官方提示：
  + 看kvminithart 以获得灵感
  + 调用 w_satp() 后，不要忘记调用 sfence_vma()
```c
// 将硬件页表寄存器切换到内核的页表，并启用分页.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}
```
+ 内核页的管理使用的是 SATP 寄存器
+ 在 kernel/proc.c 的调度函数 scheduler 中添加切换 SATP 寄存器的代码，并在调度后切换回来：
```c
// 切换到所选进程
      // 进程的工作是释放其锁，然后重新获取它 
      // 然后再跳回给我们
        p->state = RUNNING;
        c->proc = p;
        

        // 切换到进程独立的内核页表
        w_satp(MAKE_SATP(p->kpagetable));
        sfence_vma();// 清除块表缓存
        
        // 调度，执行进程
        swtch(&c->context, &p->context);

        // 重新初始化内核页表
        // 这一步是为了在从进程 p 返回到内核时，确保内核再次使用全局的内核页表，而不是进程独立的页表
        kvminithart();

      // 进程现在已运行完毕
      // 它应该在返回之前改变其 p->state
        c->proc = 0;

        found = 1;
```
### 第七步：在 freeproc 中释放进程的内核页表
+ 在进程结束后，应该释放进程独享的页表以及内核栈，回收资源，否则会导致内存泄漏
+ 释放页表的第一步是先释放页表内的内核栈
+ 因为页表内存储的内核栈地址本身就是一个虚拟地址，需要先将这个地址指向的物理地址进行释放：
+ 下面是freeproc函数本来的样子:
```c
// 释放进程资源的函数
static void freeproc(struct proc *p){
  // 如果进程有 trapframe（陷阱帧），释放它的内存
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;  // 将 trapframe 指针置为空

  // 如果进程有页表，释放它的页表
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;  // 将页表指针置为空

  // 清除进程的其他信息
  p->sz = 0;          // 将进程大小设为 0
  p->pid = 0;         // 将进程 ID 设为 0
  p->parent = 0;      // 将父进程指针设为空
  p->name[0] = 0;     // 将进程名字的第一个字符设为空（相当于清空名字）
  p->chan = 0;        // 将进程的等待通道设为空
  p->killed = 0;      // 将进程的杀死标志设为 0
  p->xstate = 0;      // 将进程的退出状态设为 0
  p->state = UNUSED;  // 将进程状态设为 UNUSED（未使用）
}
```
+ 仿照freewalk函数,写出proc_freewalk，来释放页表的映射关系
```c
void proc_freewalk(pagetable_t pagetable) {

  // 遍历页表中的512个页表项
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    
    // 如果页表项有效
    if (pte & PTE_V) {

      // 将当前页表项置零
      pagetable[i] = 0;

      // 如果页表项指向的是下级页表
      if ((pte & (PTE_R | PTE_W | PTE_X)) == 0){

        // 计算下级页表的物理地址
        uint64 child = PTE2PA(pte);

        // 递归调用proc_freewalk释放下级页表
        proc_freewalk((pagetable_t)child);
      }
    }
  }
  // 释放当前页表的内存
  kfree((void *)pagetable);
}
```
+ 并且在freeproc里面调用这个
```c
  if(p->kpagetable){
    proc_freewalk(p->kpagetable);
  }
  p->kpagetable = 0;
```

### 第八部切换进程内核页表
+ 在vm.c中添加头文件
```c
#include "spinlock.h"
#include "proc.h"
```

+ 更改kvmpa函数：
```c
// myproc函数返回的是当前进程的指针
// 这个就是获取当前进程结构体的kpagetable成员
pte = walk(myproc()->kpagetable, va, 0);
```

