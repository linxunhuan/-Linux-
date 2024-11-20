# test1/test2()：恢复被中断的代码
## 官方介绍
+ 有可能 alarmtest 在 test0 或 test1 打印 "alarm!" 之后崩溃
+ 或者 alarmtest 最终打印 "test1 failed"
+ 或者 alarmtest 在没有打印 "test1 passed" 的情况下退出
+ 要解决这个问题，必须确保当闹钟处理程序完成后，控制权返回到用户程序原本被定时中断打断的位置
+ 你必须确保寄存器内容恢复到中断时的值，以便用户程序在闹钟处理完毕后能不受干扰地继续执行
+ 最后，在每次闹钟触发后，你应该重新启动闹钟计数器，使得处理程序能够周期性地被调用
+ 作为起点，我们已经为你做了一个设计决策：
  + 用户闹钟处理程序在完成后需要调用 sigreturn 系统调用
  + 请查看 alarmtest.c 中的 periodic 作为一个例子
  + 这意味着你可以在 usertrap 和 sys_sigreturn 中添加代码，使用户进程在处理完闹钟后能够正确地恢复

## 官方提示：
+ 你的解决方案需要保存和恢复寄存器
  + 你需要保存和恢复哪些寄存器以正确恢复被中断的代码？（提示：会有很多）
+ 当定时器触发时，usertrap 需要在 struct proc 中保存足够的状态
  + 以便 sigreturn 能够正确返回被中断的用户代码
+ 防止重复调用handler:
  + 如果一个处理程序还没有返回，内核不应该再次调用它
  + test2 会测试这一点
+ 通过 test0、test1 和 test2 后，运行 usertests 以确保没有破坏内核的其他部分

## 解题步骤
### 第一步：再补充proc类
+ 中断处理完成后恢复原程序的正常执行
  + 所以设定trapframeSave项，专门来保存中断时候的函数指针
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
  
  uint64 ticks;                // 累积的alarm
  uint64 interval;             // alarm间隔时间 
  void (*alarm_function)();    // 报警处理函数

  //再新加的
  struct trapframe* trapframeSave;// 保存中断代码相关的 trapframe
  int waitReturn;              // waiting return:1;not waiting return:0
};
```
### 第二步：模仿trapframe，进行初始化和释放
<img src=".\picture\image12.png">

<img src=".\picture\image13.png">

### 第三步：trapframe有啥，trapframeSave全都粘贴过去
```c
// trap.c
void
switchTrapframe(struct trapframe* tf, struct trapframe* tfs)
{
  tf->kernel_satp = tfs->kernel_satp;
  tf->kernel_trap = tfs->kernel_trap;
  tf->kernel_sp = tfs->kernel_sp;
  tf->kernel_hartid = tfs->kernel_hartid;
  tf->epc = tfs->epc;
  tf->ra = tfs->ra;
  tf->sp = tfs->sp;
  tf->gp = tfs->gp;
  tf->tp = tfs->tp;
  tf->t0 = tfs->t0;
  tf->t1 = tfs->t1;
  tf->t2 = tfs->t2;
  tf->s0 = tfs->s0;
  tf->s1 = tfs->s1;
  tf->a0 = tfs->a0;
  tf->a1 = tfs->a1;
  tf->a2 = tfs->a2;
  tf->a3 = tfs->a3;
  tf->a4 = tfs->a4;
  tf->a5 = tfs->a5;
  tf->a6 = tfs->a6;
  tf->a7 = tfs->a7;
  tf->s2 = tfs->s2;
  tf->s3 = tfs->s3;
  tf->s4 = tfs->s4;
  tf->s5 = tfs->s5;
  tf->s6 = tfs->s6;
  tf->s7 = tfs->s7;
  tf->s8 = tfs->s8;
  tf->s9 = tfs->s9;
  tf->s10 = tfs->s10;
  tf->s11 = tfs->s11;
  tf->t3 = tfs->t3;
  tf->t4 = tfs->t4;
  tf->t5 = tfs->t5;
  tf->t6 = tfs->t6;
}
```
### 第四步：defs.h里面记得声明
<img src=".\picture\image16.png">
<img src=".\picture\image15.png">

### 第五步：完善usertrap函数
<img src=".\picture\image14.png">

### 第六步：修改sys_sigreturn
```c
uint64
sys_sigreturn(void){
  struct proc* my_proc = myproc();
  switchTrapframe(my_proc->trapframe, my_proc->trapframeSave);
  my_proc->waitReturn = 0;
  return 0;
}
```

## 测试
<img src=".\picture\image17.png">