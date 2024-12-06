# 线程间切换
## 官方介绍
+ 在本练习中，您将设计用户级线程系统的上下文切换机制，然后实现它
+ 为了帮助您入门，您的 xv6 有两个文件 user/uthread.c 和 user/uthread_switch.S，以及 Makefile 中用于构建 uthread 程序的规则
+ uthread.c 包含用户级线程包的大部分内容，以及三个简单测试线程的代码
+ 线程包缺少一些用于创建线程和在线程之间切换的代码
+ 您的任务是制定一个计划来创建线程并保存/恢复寄存器以在线程之间切换，然后实施该计划
  + 完成后， make grade应该会显示您的解决方案通过了 uthread测试
## 官方步骤
+ 该输出来自三个测试线程，每个线程都有一个循环，打印一行，然后将 CPU 交给其他线程
  + 然而，此时，由于没有上下文切换代码，您将看不到任何输出
+ 您需要在user/uthread.c中向thread_create()和 thread_schedule()添加代码
  + 在user/uthread_switch.S中向thread_switch添加代码
    + 一个目标是确保当thread_schedule()首次运行给定线程时，该线程在其自己的堆栈上执行传递给thread_create() 的函数
    + 另一个目标是确保thread_switch
      + 保存被切换离的线程的寄存器
      + 恢复被切换到的线程的寄存器
      + 并返回到后者线程指令中上次中断的位置
  + 您必须决定在何处保存/恢复寄存器
    + 修改 struct thread以保存寄存器是一个不错的计划
  + 您需要在thread_schedule中添加对thread_switch的调用
    + 您可以将所需的任何参数传递给thread_switch，但目的是从线程t切换到 next_thread
## 官方提示：
+ thread_switch只需要保存/恢复被调用者保存的寄存器。为什么？
+ 您可以在 user/uthread.asm 中看到 uthread 的汇编代码，这对于调试很方便。
+ 要测试你的代码，使用riscv64-linux-gnu-gdb单步执行你的 thread_switch可能会有所帮助。你可以这样开始：
```shell
(gdb) file user/_uthread
Reading symbols from user/_uthread...
(gdb) b uthread.c:60
```
+ 这会在uthread.c的第 60 行设置一个断点
  + 在运行uthread之前，断点可能会（也可能不会）被触发 。怎么会发生这种情况呢？
+ 一旦你的 xv6 shell 运行，输入“uthread”，gdb 将在第 60 行中断
+ 现在你可以输入以下命令来检查uthread的状态：
```shell
(gdb) p/x *next_thread
```
+ 使用“x”，您可以检查内存位置的内容：
```shell
(gdb) x/x next_thread->stack
```
+ 您可以跳至thread_switch的开始处：
```shell
(gdb) b thread_switch
(gdb) c
```
+ 您可以使用以下方式单步执行组装说明：
```shell
(gdb) si
```
## 解题步骤
+ 这里的“线程”是完全用户态实现的
  + 多个线程也只能运行在一个 CPU 上
  + 并且没有时钟中断来强制执行调度
  + 需要线程函数本身在合适的时候主动 yield 释放 CPU
+ 这样实现起来的线程并不对线程函数透明
  + 所以比起操作系统的线程而言更接近 coroutine
+ 本实验是在给定的代码基础上实现用户级线程切换
  + 相比于XV6中实现的内核级线程，这个要简单许多
  + 因为是用户级线程，不需要设计用户栈和内核栈，用户页表和内核页表等等切换
  + 所以本实验中只需要一个类似于context的结构，而不需要费尽心机的维护trapframe
------------------------------
### 第一步：定义存储上下文的结构体tcontext
```c
// 用户线程的上下文结构体
struct tcontext {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};
```
### 第二步：修改thread结构体，添加context字段
```c
struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct tcontext context;      /* 用户进程上下文 */
};
```
### 第三步：模仿kernel/swtch.S，修改kernel/uthread_switch.S
```S
thread_switch:
	/* YOUR CODE HERE */
	
	sd ra, 0(a0)
	sd sp, 8(a0)
	sd s0, 16(a0)
	sd s1, 24(a0)
	sd s2, 32(a0)
	sd s3, 40(a0)
	sd s4, 48(a0)
	sd s5, 56(a0)
	sd s6, 64(a0)
	sd s7, 72(a0)
	sd s8, 80(a0)
	sd s9, 88(a0)
	sd s10, 96(a0)
	sd s11, 104(a0)

	ld ra, 0(a1)
	ld sp, 8(a1)
	ld s0, 16(a1)
	ld s1, 24(a1)
	ld s2, 32(a1)
	ld s3, 40(a1)
	ld s4, 48(a1)
	ld s5, 56(a1)
	ld s6, 64(a1)
	ld s7, 72(a1)
	ld s8, 80(a1)
	ld s9, 88(a1)
	ld s10, 96(a1)
	ld s11, 104(a1)
	
	ret    /* return to ra */
```
### 第四步：修改thread_scheduler，添加线程切换语句
```c
if (current_thread != next_thread) {         /* switch threads?  */
    next_thread->state = RUNNING;
    t = current_thread;
    current_thread = next_thread;
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
    thread_switch((uint64)&t->context, (uint64)&current_thread->context);
  }
```
### 第五步：对thread结构体做一些初始化设定，主要是ra返回地址和sp栈指针
```c
void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE
  t->context.ra = (uint64)func;                   // 设定函数返回地址
  t->context.sp = (uint64)(t->stack + STACK_SIZE);// 设定栈指针
}
```
## 最后结果
<img src=".\picture\image1.png">





































