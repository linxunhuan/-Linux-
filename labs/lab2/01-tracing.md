# System call tracing
## 题目：
  + 在此作业中，您将添加一个系统调用跟踪功能，该功能可能有助于您调试后续的实验
  + 您将创建一个新的跟踪系统调用来控制跟踪
    + 它应该接受一个参数，即一个整数“掩码”，其为指定要跟踪哪些系统调用
    + 例如，要跟踪 fork 系统调用，程序会调用trace(1 << SYS_fork)
      + 其中SYS_fork是kernel/syscall.h中的系统调用编号
    + 如果系统调用的编号在掩码中设置，则必须修改 xv6 内核以在每个系统调用即将返回时打印出一行
      + 该行应包含进程 ID、系统调用的名称和返回值
    + 您不需要打印系统调用参数
  + 跟踪系统调用应该启用对调用它的进程及其随后分叉的任何子进程的跟踪，但不应影响其他进程
## 官方提示：
  + 在 Makefile 中将 $U/_trace添加到 UPROGS
  + 运行make qemu后，您将看到编译器无法编译user/trace.c，因为系统调用的用户空间存根尚不存在：
    + 将系统调用的原型添加到user/user.h
    + 将存根添加到user/usys.pl
    + 将系统调用编号添加到kernel/syscall.h
    + Makefile 调用 perl 脚本user/usys.pl
      + 该脚本生成user/usys.S，即实际的系统调用存根
      + 它使用 RISC-V ecall指令转换到内核
    + 修复编译问题后，运行trace 32 grep hello README；它将失败，因为您尚未在内核中实现系统调用
  + 在kernel/sysproc.c中添加一个sys_trace()函数
    + 该函数通过在proc结构中的新变量中记住其参数来实现新的系统调用（请参阅kernel/proc.h ）
    + 从用户空间检索系统调用参数的函数位于kernel/syscall.c中，您可以在kernel/sysproc.c中看到它们的使用示例
  + 修改fork()（参见kernel/proc.c）以将跟踪掩码从父进程复制到子进程
  + 修改kernel/syscall.c中的syscall()函数以打印跟踪输出。您需要添加一个系统调用名称数组以进行索引

## 做题思路
+ 首先介绍一下xv6的执行顺序
<img src=".\picture\image2.png">

+ 所以，结合官方的提示，我们知道需要在各个文件中定义函数，下面依次介绍：
+ 在user/user.h上添加trace函数的声明
  + 使得用户态程序可以找到跳板入口函数
<img src=".\picture\image3.png">

+ 在user/usys.pl脚本中加入entry("trace")函数
  + 这是从用户态到内核态的跳板函数
  
    
<img src=".\picture\image4.png">

  + 这个脚本会生成risc-v汇编代码user/usys.S
    + 里面定义了每个 system call 的用户态跳板函数：
<img src=".\picture\image8.png">


+ 如此繁琐的调用流程的主要目的——实现用户态和内核态的良好隔离
+ 并且由于内核与用户进程的页表不同
  + 寄存器也不互通
  + 参数无法直接通过 C 语言参数的形式传过来
  + 所以，需要使用 argaddr、argint、argstr 等系列函数，从进程的 trapframe 中读取用户进程寄存器中的参数
-----
+ 首先写内核系统调用的模块
```c
uint64
sys_trace(void){
  int pid;

  // argint()用于读取在a0-a5寄存器中传递的系统调用参数
  if(argint(0, &pid) < 0){
    return -1;
  }

  // myproc()函数获取当前进程的PCB
  myproc()->mask = pid;
  return 0;
}
```
+ 在kernel/syscall.h中添加sys_trace的序号
<img src=".\picture\image5.png">

+ 用 extern 全局声明新的内核调用函数，并且在 syscalls 映射表中，加入从前面定义的编号到系统调用函数指针的映射
  + 在kernel/syscall.c中外部全局声明新的内核调用函数sys_trace
<img src=".\picture\image7.png">

  + 在kernel/syscall.c的映射表syscalls中添加一个编号到系统调用指针
<img src=".\picture\image6.png">

+ 这里 [SYS_trace] sys_trace 是 C 语言数组的一个语法
  + 表示以方括号内的值作为元素下标
  + 比如 int arr[] = {[3] 2333, [6] 6666} 
    + arr 的下标 3 的元素为 2333，
    + 下标 6 的元素为 6666，其他元素填充 0 的数组
+ 该语法在 C++ 中已不可用

+ 在 proc.h 中修改 proc 结构的定义，添加 mask，记录要 trace 的 system call
```c
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // 如果不为零，则sleeing on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // 退出状态返回给父进程等待
  int pid;                     // Process ID

  // 这些对于进程来说是私有的，因此不需要持有 p->lock
  uint64 kstack;               // 内核堆栈的虚拟地址
  uint64 sz;                   // 进程内存的大小（字节）
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // trampoline.S 的数据页面
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // 进程名称（调试）
  int mask;                    // 存储跟踪号
};
```

+ 在 sysproc.c 中，实现 system call 的具体代码，也就是设置当前进程的 syscall_trace mask：
```c
uint64
sys_trace(void){
  int pid;

  // argint()用于读取在a0-a5寄存器中传递的系统调用参数
  if(argint(0, &pid) < 0){
    return -1;
  }

  // myproc()函数获取当前进程的PCB
  myproc()->mask = pid;
  return 0;
}
```
+ 修改 fork 函数，使得子进程可以继承父进程的 mask：
```c
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // 分配新进程
  if((np = allocproc()) == 0){
    return -1;
  }

  // 将用户内存从父级复制到子级.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);       // 如果复制失败，释放新进程
    release(&np->lock); // 释放锁
    return -1;
  }
  np->sz = p->sz;       // 设置子进程的大小与父进程相同

  np->parent = p;       // 设置子进程的父进程为当前进程

  // 复制已保存的用户注册表
  *(np->trapframe) = *(p->trapframe);

  // a0 寄存器通常用于存储函数的返回值
  // 使fork 在子进程中返回 0
  np->trapframe->a0 = 0;

  // 增加打开文件描述符的引用计数—— np 将继承p 的所有打开文件
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd); // 复制当前工作目录

  safestrcpy(np->name, p->name, sizeof(p->name));//复制进程名字

  np->mask = p->mask;
  pid = np->pid;           // 获取新进程的PID

  np->state = RUNNABLE;    // 设置新进程状态为可运行

  release(&np->lock);

  return pid;
}
```
+ 所有的系统调用到达内核态后，都会进入到 syscall() 这个函数进行处理
+ 所以要跟踪所有的内核函数，只需要在 syscall() 函数里埋点
```c
void
syscall(void)
{
  int num;// 存储系统调用号
  struct proc *p = myproc();//获取当前进程的PCB

  // 从当前进程的trapframe中获取系统调用号
  num = p->trapframe->a7;
  
  // 检查系统调用号是否有效，并且在syscalls 数组中有对应的处理函数 
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    
    // 调用对应的系统调用处理函数，并将返回值存储在a0寄存器中
    p->trapframe->a0 = syscalls[num]();

    // 如果当前进程的mask字段中对应的位被设置，则打出 pid、系统调用名称和返回值
    if(p->mask & (1 << num)){
      printf("%d: syscall %s -> %d\n",p->pid, p->name[num], p->trapframe->a0);
    }
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```
最终编译结果，符合题目要求：
<img src=".\picture\image9.png">