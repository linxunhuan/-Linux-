# 警告
## 题目介绍
+ 在本练习中，您将向 xv6 添加一项功能，该功能会在进程使用 CPU 时间时定期提醒它
  + 这可能对计算受限的进程很有用，这些进程希望限制它们占用的 CPU 时间
  + 或者对想要计算但也想要采取一些定期操作的进程很有用
  + 更一般地说，您将实现一种原始形式的用户级中断/故障处理程序；
    + 例如，您可以使用类似的东西来处理应用程序中的页面错误
    + 如果您的解决方案通过了警报测试和用户测试，则说明它是正确的
## 题目步骤
+ 您应该添加一个新的 sigalarm(interval, handler) 系统调用
+ 如果应用程序调用 sigalarm(n, fn)
  + 在程序消耗的每 n 个 CPU 时间“ticks”之后，内核应该导致调用应用程序函数 fn
+ 当 fn 返回时，应用程序应该从中断的地方恢复
  + ticks是 xv6 中相当随意的时间单位，由硬件计时器生成中断的频率决定
  + 如果应用程序调用 sigalarm(0, 0)，内核应该停止生成定期警报调用
+ 您将在 xv6 存储库中找到文件 user/alarmtest.c
  + 将其添加到 Makefile
+ 除非您添加了 sigalarm 和 sigreturn 系统调用（见下文），否则它无法正确编译
+ alarmtest 在 test0 中调用 sigalarm(2, periodic)
  + 要求内核每 2 个ticks,强制调用 periodic() 一次，然后旋转一段时间
  + 您可以在 user/alarmtest.asm 中看到 alarmtest 的汇编代码，这对调试很有帮助
  + 当 alarmtest 产生如下输出并且 usertests 也能正确运行时，您的解决方案是正确的：
```shell
$ alarmtest
test0 start
........alarm!
test0 passed
test1 start
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
test1 passed
test2 start
................alarm!
test2 passed
$ usertests
...
ALL TESTS PASSED
$
```
+ 完成后，您的解决方案将只有几行代码，但要正确完成可能有些困难
+ 我们将使用原始存储库中的 alarmtest.c 版本测试您的代码
+ 您可以修改 alarmtest.c 以帮助您进行调试，但请确保原始 alarmtest 表明所有测试都通过

## test0：调用处理程序官方提示
+ 首先修改内核，使其跳转到用户空间中的警报处理程序，这将导致 test0 打印“alarm!”
  + 不要担心“alarm!”输出后会发生什么
  + 如果您的程序在打印“alarm!”后崩溃，现在还算正常
+ 您需要修改 Makefile 以使alarmtest.c 被编译为 xv6 用户程序
+ 应放入user/user.h中的正确声明是
```c
int sigalarm(int ticks，void(*handler)());
int sigreturn(void);
```
+ 更新 
  + user/usys.pl（生成 user/usys.S）
  + kernel/syscall.h
  + kernel/syscall.c 
  + 以允许alarmtest调用 sigalarm 和 sigreturn 系统调用
+ 现在，您的sys_sigreturn应该只返回零
+ 您的sys_sigalarm()应该将警报间隔和指向处理程序函数的指针存储在proc 结构中的新字段中（在kernel/proc.h中）
+ 您需要跟踪自上次调用（或到下次调用）进程的警报处理程序以来已经过的滴答数
+ 您还需要在struct proc中创建一个新字段来实现此目的
  + 您可以 在proc.c中的allocproc()中初始化proc字段
+ 每一次滴答，硬件时钟都会强制中断，这由kernel/trap.c中的usertrap()处理
+ 只有在有定时器中断的情况下，你才想操纵进程的警报滴答声；你想要类似的东西
```c
 if(which_dev == 2) ...
```
+ 仅当进程有未完成的计时器时才调用警报函数
  + 请注意，用户的警报函数的地址可能是 0
  + （例如，在 user/alarmtest.asm 中，periodial位于地址 0）
+ 您需要修改 usertrap()，以便当进程的警报间隔到期时，用户进程执行处理程序函数
  + 当 RISC-V 上的陷阱返回到用户空间时，什么决定了用户空间代码恢复执行的指令地址？
+ 如果你告诉 qemu 只使用一个 CPU，那么使用 gdb 查看陷阱会更容易，你可以通过运行
make CPUS=1 qemu-gdb
+ 如果 alarmtest 打印“alarm!”则表示您成功了

## 解题思路
### 第一步：将sigalarm和sigreturn添加到系统调用中
+ 这就和一开始实验步骤一样， 添加到各个文件中
+ user/usys.pl
<img src=".\picture\image8.png">

+ kernel/syscall.h
<img src=".\picture\image9.png">

+ kernel/syscall.c
<img src=".\picture\image10.png">

### 第二步：文件 user/alarmtest.c添加到Makefile
<img src=".\picture\image7.png">

### 第三步：完成sys_sigalarm()和sys_sigreturn()函数
```c
/*这个函数是为了执行sigalarm(n, fn)，所以需要函数指针*/
uint64
sys_sigalarm(void){
  struct proc* my_proc = myproc();
  int n;                // 表示用户传入的时间间隔
  uint64 function_addr; // 表示用户传入的信号处理函数的地址

  // 验证用户传递进来的参数。
  if(argint(0,&n) < 0 || argaddr(1,&function_addr) < 0){
    return -1;
  }
  my_proc->interval = n;
  my_proc->alarm_function = (void(*)())function_addr;
  return 0;
}

/*提示说的*/
uint64
sys_sigreturn(void){
  return 0;
}
```

### 第四步：添加ticks记录时间
+ 上面的步骤是为了呼应测试程序的
+ 下面的几步才是完成具体功能的
+ 这里需要对整个类都要有详细的了解，并且加上ticks这几项
```c
struct proc {

  struct spinlock lock;

  // 使用这些时必须持有 p->lock：
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // 如果不为零，表示进程正在等待某个资源（睡眠在 chan 上）
  int killed;                  // 如果不为零，表示进程已被标记为需要终止
  int xstate;                  // 进程退出状态，父进程通过 wait() 获取
  int pid;                     // 进程 ID（唯一标识一个进程）

  // 以下字段是进程私有的，访问这些字段时不需要持有 p->lock：
  uint64 kstack;               // 内核栈的虚拟地址，用于保存进程运行时的内核栈
  uint64 sz;                   // 进程占用的内存大小 (bytes)
  pagetable_t pagetable;       // 进程的用户页表，用于内存管理
  struct trapframe *trapframe; // 中断帧，保存用户态寄存器的状态，用于中断返回。data page for trampoline.S
  struct context context;      // 进程的上下文（context），保存 CPU 寄存器状态，用于切换进程
  struct file *ofile[NOFILE];  // 打开的文件表，最多可以打开 NOFILE 个文件
  struct inode *cwd;           // 当前工作目录，指向 inode 结构
  char name[16];               // 进程名称（用于调试，最多 15 个字符加 1 个空终止符）(debugging)
  
  // 以下是新添加的项
  uint64 ticks;                // 累积的alarm
  uint64 interval;             // alarm间隔时间 
  void (*alarm_function)();    // 报警处理函数
};
```
+ 在这个proc.h文件里面，还有一个类要介绍一部分
```c
/**
 * 用于处理用户态到内核态的中断和异常的关键数据结构
 * 每个进程都有一个独立的 trapframe
 * 它用于保存用户态寄存器的状态，并存储一些与内核处理相关的信息
 * trapframe 位于用户页表中，专门占用一页内存
 * 它的设计目的是让内核能够在用户态发生中断时，保存和恢复用户态的上下文
 */
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // 内核的页表基地址
  /*   8 */ uint64 kernel_sp;     // 进程内核栈的栈顶指针(这是处理中断时使用的栈)
  /*  16 */ uint64 kernel_trap;   // 内核中断处理函数的入口地址usertrap()
  /*  24 */ uint64 epc;           // 用户态的程序计数器 (PC)
                                  // 保存用户程序中断或异常发生时的指令地址
                                  // 返回用户态时恢复这个值以继续执行
  /*  32 */ uint64 kernel_hartid; // 内核的硬件线程 ID (hart ID)
                                  // 保存当前 CPU 核心的标识符，用于内核处理逻辑
}
```
### 完善usertrap()函数
```c
//
// 处理来自用户空间的中断、异常或系统调用。
// 从 trampoline.S 调用
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // 将中断和异常发送到 kerneltrap()，
  // 因为我们现在在内核中
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // 保存用户程序计数器.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc 指向 ecall 指令，但我们想返回下一条指令.
    p->trapframe->epc += 4;

    // 中断将改变 sstatus 和 c 寄存器，
    // 因此在完成这些寄存器之前不要启用.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
    if (which_dev == 2){
      p -> ticks = p->ticks + 1;
      if(p->ticks == p -> interval){
        p -> ticks = 0;
        p ->trapframe ->epc = (uint64)p->alarm_function;
      }
    }
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // 如果这是一个定时器中断，则放弃 CPU.
  if(which_dev == 2)
    yield();

  usertrapret();
}
```
## 执行结果
<img src=".\picture\image11.png">
可以看到test0已经passed
test1和test2将在下一页完成














