## 题目：
----------
+ 内核的 copyin函数读取用户指针指向的内存
  + 它通过将这些内存转换为物理地址来实现这一点，内核可以直接取消引用这些物理地址
  + 它通过遍历软件中的进程页表来执行此转换
+ 您在本部分实验中的工作是向每个进程的内核页表（在上一节中创建）添加用户映射
  + 以允许 copyin（以及相关的字符串函数 copyinstr）直接取消引用用户指针
----------
+ 将kernel/vm.c中的copyin 函数体替换为
    对copyin_new的调用（在kernel/vmcopyin.c中定义 ）
+ 对copyinstr和copyinstr_new执行相同操作
+ 将用户地址的映射添加到每个进程的内核页表，以便 copyin_new和copyinstr_new可以正常工作
+ 如果usertests运行正常并且所有make grade测试都通过， 则您通过了此作业
-----------------
+ 该方案依赖于用户虚拟地址范围不与内核用于其自身指令和数据的虚拟地址范围重叠
+ xv6 使用从零开始的虚拟地址作为用户地址空间
  + 内核的内存从更高的地址开始
+ 但是，该方案确实将用户进程的最大大小,限制为小于内核的最低虚拟地址
+ 内核启动后，该地址在 xv6 中为 0xC000000
  + 即 PLIC 寄存器的地址
+ 请参阅 kernel/vm.c 中的 kvminit()、kernel/memlayout.h 和文本中的图 3-4
+ 您需要修改 xv6 以防止用户进程增长到大于 PLIC 地址

## 官方提示
+ 首先用对 copyin_new 的调用替换 copyin()，并使其工作，然后再转到 copyinstr
+ 在内核更改进程的用户映射的每个点，以相同的方式更改进程的内核页表
  + 这些点包括 fork()、exec() 和 sbrk()
+ 不要忘记在 userinit 中将第一个进程的用户页表包含在其内核页表中
+ 进程的内核页表中的用户地址的 PTE 需要什么权限？
  + （设置了 PTE_U 的页面无法在内核模式下访问。）
+ 不要忘记上面提到的 PLIC 限制
----------------
+ Linux 使用的技术与您实施的技术类似
+ 直到几年前，许多内核在用户空间和内核空间中使用相同的每个进程页表，并映射用户和内核地址
  + 以避免在用户空间和内核空间之间切换时必须切换页表
+ 然而，这种设置允许诸如 Meltdown 和 Spectre 之类的旁道攻击

## 做题步骤
### 做题理解
+ 在上一个实验中，已经使得每一个进程都拥有独立的内核态页表了
+ 这个实验的目标是，在进程的内核态页表中维护一个用户态页表映射的副本，这样使得内核态也可以对用户态传进来的指针（逻辑地址）进行解引用
+ 这样做相比原来 copyin 的实现的优势是
  + 原来的 copyin 是通过软件模拟访问页表的过程获取物理地址的
  + 在内核页表内维护映射副本的话，可以利用 CPU 的硬件寻址功能进行寻址，效率更高并且可以受快表加速
+ 要实现这样的效果，我们需要在每一处内核对用户页表进行修改的时候，将同样的修改也同步应用在进程的内核页表上
  + 使得两个页表的程序段（0 到 PLIC 段）地址空间的映射同步

### 第一步：复制页表内容
+ 记得在defs.h中声明函数
+ 首先是构建复制函数用于pagetable的复制
  + 其中要注意就是因为pagetable的叶节点，也就是PTE的标志位设置PTE_U
  + 进入内核时无法使用该地址，需要先清楚再放入kernel pagetable里
+ 这里的代码，我借鉴了uvmcopy函数 和uvmalloc函数
```c
// 给定父进程的页表，将其内存复制到子进程的页表中 
// 复制页表和物理内存
// 成功时返回 0，失败时返回 -1
// 失败时释放所有分配的页面

/**
 * @brief 将旧页表（old）中的虚拟地址空间复制到新页表（new）
 * @param old 源页表，表示现有虚拟地址空间的页表
 * @param new 目标页表，用于接收复制后的虚拟地址空间的页表
 * @param sz 复制的虚拟地址空间大小，以字节为单位
 * @return 返回 0 表示复制成功，返回 -1 表示复制失败
 */
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
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
-------
```c
// 分配 PTE 和物理内存，使进程从 oldsz 增长到newsz，无需页面对齐
// 返回新大小，如果出错则返回 0.

/**
 * @brief 在页表中分配一段连续的虚拟地址空间，并将其映射到实际的物理内存
 * @param pagetable 页表的起始地址
 * @param oldsz 现有虚拟地址空间的大小
 * @param newsz 新的虚拟地址空间的大小
 */
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}
```
+ 下面是我写的代码：
```c
void
u2kvmcopy(pagetable_t pagetable, pagetable_t kpagetable, uint64 oldsz, uint64 newsz)
{
  pte_t *pte_from, *pte_to;
  uint64 a, pa;
  uint flags;

  if (newsz < oldsz)
    return;
  
  // 将旧大小向上对齐到页面大小的倍数
  oldsz = PGROUNDUP(oldsz);
  for (a = oldsz; a < newsz; a += PGSIZE)
  {
    // 获取用户页表中虚拟地址 a 对应的页表项
    if ((pte_from = walk(pagetable, a, 0)) == 0)
      panic("u2kvmcopy: pte should exist");
    if ((pte_to = walk(kpagetable, a, 1)) == 0)
      panic("u2kvmcopy: walk fails");
    // 从用户页表项中获取物理地址
    pa = PTE2PA(*pte_from);
    // 获取用户页表项的标志，并移除用户标志 PTE_U
    flags = (PTE_FLAGS(*pte_from) & (~PTE_U));

    // 将物理地址映射到内核页表中的虚拟地址，并设置标志
    *pte_to = PA2PTE(pa) | flags;
  }
}
```

### 第二步：在内核更改进程的用户映射的每个点
+ 官方说了，包括fork()、exec() 和 sbrk()和userinit()函数
```c
// fork.c

// 创建一个新进程，复制父进程。
// 设置子进程的内核栈，使其看起来像是从 fork() 系统调用返回。
int
fork(void)
{
  int i, pid;
  struct proc *np;         // 新进程结构体指针
  struct proc *p = myproc(); // 当前进程（父进程）结构体指针

  // 分配新进程结构体。
  if((np = allocproc()) == 0){
    return -1;  // 分配失败，返回 -1
  }

  // 从父进程复制用户内存到子进程。
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);          // 内存复制失败，释放新进程
    release(&np->lock);    // 释放锁
    return -1;             // 返回 -1
  }
  np->sz = p->sz;          // 设置新进程的大小

  np->parent = p;          // 设置新进程的父进程指针

  // 复制保存的用户寄存器。
  *(np->trapframe) = *(p->trapframe);

  // 使 fork 在子进程中返回 0。
  np->trapframe->a0 = 0;

  // 增加打开文件描述符的引用计数。
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);  // 复制当前工作目录

  // 复制用户页表到内核页表。
  u2kvmcopy(np->pagetable, np->kpagetable, 0, np->sz);

  // 复制进程名称。
  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;           // 获取新进程的 PID

  np->state = RUNNABLE;    // 设置新进程为可运行状态

  release(&np->lock);      // 释放新进程的锁

  return pid;              // 返回新进程的 PID
}
```
```c
int exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG+1], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc(); // 获取当前进程

  begin_op(); // 开始文件系统操作

  // 通过路径查找文件节点
  if((ip = namei(path)) == 0){
    end_op(); // 结束文件系统操作
    return -1; // 如果找不到文件，返回 -1
  }
  ilock(ip); // 锁定文件节点

  // 检查 ELF 文件头
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  // 创建新的页表
  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // 将程序加载到内存
  for(i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    uint64 sz1;
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    sz = sz1;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip); // 解锁并释放文件节点
  end_op(); // 结束文件系统操作
  ip = 0;

  p = myproc(); // 重新获取当前进程
  uint64 oldsz = p->sz; // 保存旧的内存大小

  // 分配两个页面，使用第二个页面作为用户栈
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz - 2*PGSIZE);
  sp = sz;
  stackbase = sp - PGSIZE;

  // 复制页表到内核页表
  u2kvmcopy(pagetable, p->kpagetable, 0, sz);

  // 将参数字符串压入栈，并准备其余的栈内容
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp 必须是 16 字节对齐的
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // 将 argv[] 指针数组压入栈
  sp -= (argc + 1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc + 1) * sizeof(uint64)) < 0)
    goto bad;

  // 将参数传递给用户 main(argc, argv)
  p->trapframe->a1 = sp;

  // 保存程序名称以供调试
  for(last = s = path; *s; s++)
    if(*s == '/')
      last = s + 1;
  safestrcpy(p->name, last, sizeof(p->name));

  // 提交用户图像
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // 初始程序计数器 = main
  p->trapframe->sp = sp; // 初始栈指针
  proc_freepagetable(oldpagetable, oldsz); // 释放旧的页表

  vmprint(p->pagetable); // 打印页表信息

  return argc; // 返回 argc，这将在 a0 中作为 main(argc, argv) 的第一个参数

bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1; // 出现错误时返回 -1
}
```
```c
// 设置第一个用户进程
void
userinit(void)
{
  struct proc *p;

  // 分配一个新进程结构体
  p = allocproc();
  initproc = p; // 将新分配的进程设为初始进程
  
  // 分配一个用户页面并将初始代码和数据复制到其中
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE; // 设置进程大小为一个页面
  u2kvmcopy(p->pagetable, p->kpagetable, 0, p->sz); // 复制页表到内核页表

  // 准备从内核返回到用户态。
  p->trapframe->epc = 0;      // 用户程序计数器，设为0
  p->trapframe->sp = PGSIZE;  // 用户栈指针，设为页面大小

  // 设置进程名称为 "initcode"
  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/"); // 设置当前工作目录为根目录

  // 将进程状态设置为可运行
  p->state = RUNNABLE;

  // 释放进程锁
  release(&p->lock);
}
```

### 修改copyin()和copinstr()函数
+ 这里很坑的是，copyin_new()和copyinstr_new()都是题目自己给的，在vmcopyin.c文件中
  + 但结果defs.h文件中根本没有声明，所以还得自己去加
```c
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  return copyin_new(pagetable, dst, srcva, len);
}
```
```c
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max){
  return copyinstr_new(pagetable, dst, srcva, max);
}
```



















