# 多线程
## 官方介绍
+ 本实验将帮助您熟悉多线程。您将在用户级线程包中实现线程之间的切换，使用多个线程来加速程序，并实现屏障
+ 在编写代码之前，您应该确保您已经阅读了xv6书中的“第7章：调度”并研究了相应的代码
## 第七章：调度
+ 任何操作系统运行的进程数量都可能超过计算机的CPU数量，因此需要制定一个方案，在各进程之间分时共享CPU
+ 理想情况下，这种共享对用户进程是透明的
+ 一种常见的方法是通过将进程复用到硬件CPU上，给每个进程提供它有自己的虚拟CPU的假象
+ 本章解释xv6如何实现这种复用
### Multiplexing（多路复用技术）
+ xv6通过在两种情况下将CPU从一个进程切换到另一个进程来实现复用
+ 首先，xv6的sleep和wakeup机制会进行切换
  + 这会发生在进程等待设备或管道I/O
  + 或等待子进程退出
  + 或在sleep系统调用中等待
+ 其次，xv6周期性地强制切换，以应对长时间不进行sleep操作的计算进程
+ 这种复用造成了每个进程都有自己的CPU的假象，就像xv6使用内存分配器和硬件页表造成每个进程都有自己的内存的假象一样
+ 实现复用会有一些挑战
  + 首先，如何从一个进程切换到另一个进程？
    + 虽然上下文切换的想法很简单，但在XV6的实现中上下文切换却是最不透明的代码之一
  + 第二，如何以对用户进程透明的方式进行强制切换？
    + xv6采用标准通用的方式，用定时器中断来驱动上下文切换
  + 第三，许多CPU可能会在进程间并发切换，需要设计一个锁来避免竞争
  + 第四，当进程退出时，必须释放进程的内存和其他资源
    + 但进程本身不能完全释放掉所有的资源，比如它不能在使用内核栈的同时释放自己的内核栈
  + 第五，多核机器的每个内核必须记住它正在执行的进程
    + 这样系统调用才能修改相应进程的内核状态
  + 最后，sleep和wakeup允许一个进程放弃CPU，并睡眠等待某一事件，并允许另一个进程将睡眠的进程唤醒
    + 需要注意一些竞争可能会使唤醒丢失

### 代码：上下文切换
<img src=".\picture\image.png">

+ 图7.1概述了从一个用户进程切换到另一个用户进程所涉及的步骤：
  + 用户-内核的切换（通过系统调用或中断）到旧进程的内核线程
  + 上下文（context）切换到当前CPU的调度器线程
  + 上下文（context）切换到新进程的内核线程
  + trap返回到用户级进程
+ xv6调度器在每个CPU上有一个专门的线程(保存了寄存器和栈)，因为调度器在旧进程的内核栈上执行是不安全的：
  + 因为其他核心可能会唤醒该进程并运行它
  + 而在两个不同的核心上使用相同的栈将是一场灾难
  + 在本节中，我们将研究在内核线程和调度线程之间切换的机制
+ 从一个线程切换到另一个线程，需要保存旧线程的CPU寄存器，并恢复新线程之前保存的寄存器
  + 栈指针和pc被保存和恢复，意味着CPU将切换栈和正在执行的代码
+ 函数swtch执行内核线程切换的保存和恢复
  + swtch并不直接知道线程，它只是保存和恢复寄存器组，称为上下文(context)
  + 当一个进程要放弃CPU的时候，进程的内核线程会调用swtch保存自己的上下文并返回到调度器上下文
  + 每个上下文都包含在一个结构体 context(kernel/proc.h:2)中
    + 它本身包含在进程的结构体proc或CPU的结构体cpu中
  + Swtch有两个参数：struct context old和struct context new
    + 它将当前的寄存器保存在old中，从new中加载寄存器，然后返回
+ 在中断结束时，有一种情况是usertrap调用yield
  + yield又调用sched，sched调用swtch将当前上下文保存在p->context中
  + 并切换到之前保存在cpu->scheduler中的调度器上下文（kernel/proc.c:509）
+ Swtch(kernel/swtch.S:3)只保存callee-saved寄存器
  + caller-saved寄存器由调用的C代码保存在堆栈上(如果需要)
  + Swtch知道struct context中每个寄存器字段的偏移量
  + 它不保存pc。相反，swtch保存了ra寄存器[1]，它保存了swtch应该返回的地址
  + 现在，swtch从新的上下文中恢复寄存器，新的上下文中保存着前一次swtch所保存的寄存器值
  + 当swtch返回时，它返回到被恢复的ra寄存器所指向的指令，也就是新线程之前调用swtch的指令
  + 此外，它还会返回新线程的堆栈
+ 在我们的例子中，sched调用swtch切换到cpu->scheduler，即CPU调度器的上下文
  + 这个上下文已经被scheduler对swtch的调用所保存(kernel/proc.c:475)
  + 当我们跟踪的swtch返回时，它不是返回到sched而是返回到scheduler
  + 它的栈指针指向当前CPU的调度器栈
+ 根据XV6的源代码，xv6中只有两处调用switch：
```c
void
sched(void)
{
   // ...
   swtch(&p->context, &mycpu()->scheduler);
   // ...
}
```
```c
void
scheduler(void)
{
    // ...
    swtch(&c->scheduler, &p->context);
    // ...
}
```
+ 可以看出这里没有两个用户进程之间的直接切换,只有用户进程和调度器线程之间的切换：
  + xv6中要主动让出cpu的进程都是通过调用exit/sleep/yield，间接调用sched
  + 从而实现切换到调度器线程，再由调度器线程选出并切换到一个runnable
###  Code: Scheduling(代码：调度)
+ 调度器以CPU特殊线程（每个CPU各一个）的形式存在，线程运行scheduler函数
+ 这个函数负责选择下一步运行哪个进程
+ 一个想要放弃CPU的进程
  + 必须获取自己的进程锁p->lock
  + 释放它所持有的其他锁
  + 更新自己的状态（p->state）
  + 然后调用sched
+ Sched对这些条件进行仔细检查(kernel/proc.c:499-504)，然后再检查这些条件的含义：
  + 既然锁被持有，就应该禁用中断
  + 最后，sched调用swtch将当前上下文保存在p->context中，并切换到cpu->scheduler中scheduler的上下文
  + Swtch在scheduler堆栈上返回，scheduler继续for循环，找到一个要运行的进程，切换到它，然后循环重复
+ 我们刚刚看到xv6在调用swtch的过程中持有p->lock：
  + swtch的调用者必须已经持有锁，并把锁的控制权移交给切换到的代码
  + 这种约定对于锁来说是不寻常的
    + 一般来说获得锁的线程也要负责释放锁，这样才容易保证正确性
  + 对于上下文切换来说，有必要打破这个约定
    + 因为p->lock保护了进程的状态和context字段上的不变量（invariant）
    + 而这些不变量在swtch执行时是不正确的
    + 如果p->lock在swtch过程中不被持有，可能会出现问题的一个情况：
      + 在yield将其状态设置为RUNNABLE之后，但在swtch切换到新的栈之前，其他CPU可能会运行这个进程
      + 结果就是两个CPU运行在同一个栈上，这显然是错误的
+ 一个内核线程在sched中放弃它的CPU，并且切换到scheduler的同一个位置
  + 而scheduler（几乎）总是切换到之前调用sched的某个内核线程
  + 因此，如果把xv6切换线程的行号打印出来，就会观察到下面的结果：
    + (kernel/proc.c:475)，(kernel/proc.c:509)，(kernel/proc.c:475)，(kernel/proc.c:509)，等等
    + 在两个线程之间发生这种样式化切换的程序有时被称为协程（coroutine）
    + 在这个例子中，sched和scheduler是彼此的coroutines
+ 有一种情况是调度器对swtch的调用没有以sched结束
  + 当一个新进程第一次被调度时，它从forkret开始（kernel/proc.c:527）。
    + forkret的存在是为了释放p->lock
    + 否则，新进程需要从usertrapret开始
+ scheduler(kernel/proc.c:457)运行了一个简单的循环：
  + 找到一个可以运行进程，运行它，直到它让出CPU，一直重复
  + 调度器在进程表上循环寻找一个可运行的进程，即p->state == RUNNABLE的进程
  + 一旦找到这样的进程，它就会设置CPU当前进程变量c->proc指向该进程
    + 将该进程标记为RUNNING
    + 然后调用swtch开始运行它(kernel/proc.c:470- 475)
+ 你可以这样理解调度代码结构
  + 它执行一组关于进程的不变量，并且每当这些不变量不正确时，就持有p->lock
  + 一个不变量是，如果一个进程正在运行，那么定时中断导致的yield必须能够安全的让他让出cpu
    + 这意味着CPU寄存器必须持有该进程的寄存器值（即swtch没有将它们移到上下文中），并且c->proc必须指向该进程
  + 另一个不变量是，如果一个进程是RUNNABLE的，那么对于一个空闲的CPU调度器来说，运行它必须是安全的
  + 这意味着:
    + p->context必须拥有进程的寄存器（即它们实际上并不在真实的寄存器中）
    + 没有CPU在进程的内核栈上执行
    + 也没有CPU的c->proc指向该进程
  + 请注意，当p->lock被持有时，这些属性往往不正确
+ 维护上述不变量是xv6经常在一个线程中获取p->lock
  + 然后在另一个线程中释放它的原因（例如在yield中获取，在schedululer中释放）
  + 一旦yield开始修改一个正在运行的进程的状态，使其成为RUNNABLE，锁必须一直保持，直到不变量被恢复：
    + 最早正确的释放点是在调度器（运行在自己的堆栈上）清除c->proc之后
  + 同样，**一旦调度器开始将一个RUNNABLE进程转换为RUNNING，锁就不能被释放，直到内核线程完成运行**（在swtch之后，例如在yield中）
+ p->lock也保护其他的东西：
  + exit和wait之间的相互作用
    + 避免丢失唤醒的机制
    + 避免退出进程和读写其状态的其他进程之间的竞争
      + （例如，exit系统调用查看p->pid并设置p->killed (kernel/proc.c:611)
### Code: mycpu and myproc
+ ​ Xv6经常需要一个指向当前进程proc的指针
  + 在单核处理器上，可以用一个全局变量指向当前的proc
  + 这在多核机器上是行不通的，因为每个核都执行不同的进程
    + 解决这个问题的方法是利用每个核都有自己的一组寄存器的事实
    + 我们可以使用其中的一个寄存器来帮助查找每个核的信息
+ Xv6为每个CPU维护了一个cpu结构体(kernel/proc.h:22)
  + 它记录了:
    + 当前在该CPU上运行的进程(如果有的话)
    + 为CPU的调度线程保存的寄存器
    + 管理中断禁用所需的嵌套自旋锁的计数
  + 函数mycpu(kernel/proc.c:60)返回一个指向当前CPU结构体cpu的指针
  + RISC-V对CPU进行编号，给每个CPU一个hartid
  + Xv6确保每个CPU的hartid在内核中被存储在该CPU的tp寄存器中
    + 这使得mycpu可以使用tp对cpu结构体的数组进行索引，从而找到正确的cpu
  + 确保一个CPU的tp始终保持CPU的hartid是有一点复杂的
    + mstart在CPU启动的早期设置tp寄存器，此时CPU处于机器模式(kernel/start.c:46)
    + Usertrapret将tp寄存器保存在trampoline页中，因为用户进程可能会修改tp寄存器
    + 最后，当从用户空间进入内核时，uservec会恢复保存的tp（trapframe中的tp加载到tp寄存器）(kernel/trampoline.S:70)
    + 编译器保证永远不使用tp寄存器
    + 如果RISC-V允许xv6直接读取当前的hartid会更方便，但这只允许在机器模式下读取，而不允许在管理模式下读取
  + cpuid和mycpu的返回值很容易错：
    + 如果定时器中断，导致线程让出CPU，然后转移到不同的CPU上，之前返回的值将不再正确
    + 为了避免这个问题，xv6要求调用者禁用中断，只有在使用完返回的cpu结构后才启用中断
      + (即为了避免这个问题，调用cpuid和mycpu时，需要禁用中断)
+ myproc(kernel/proc.c:68)函数返回当前CPU上运行的进程的proc指针
  + myproc禁用中断，调用mycpu，从struct cpu中获取当前进程指针(c->proc)
  + 然后启用中断。即使启用了中断，myproc的返回值也可以安全使用：
    + 如果定时器中断将调用进程移到了另一个的CPU上，它的proc结构指针将保持不变
###  Sleep and wakeup
+ 调度和锁有助于让一个进程对另一个进程的不可见，但到目前为止，我们还没有帮助进程进行交互的抽象
  + Xv6使用了一种叫做睡眠和唤醒的机制
    + 它允许一个进程睡眠并等待事件
    + 另一个进程在该事件发生后将其唤醒
  + 睡眠和唤醒通常被称为序列协调（sequence coordination） 或条件同步（conditional synchronization） 机制
+ 为了说明这一点，让我们考虑一个叫做信号量（semaphore）[4]的同步机制，它协调生产者和消费者
  + 信号量维护一个计数并提供两个操作
    + V操作（针对生产者）增加计数
    + P操作（针对消费者）等待，直到计数非零，然后将其递减并返回
    + 如果只有一个生产者线程和一个消费者线程，而且它们在不同的CPU上执行，编译器也没有太过激进的优化
```c
struct semaphore
{
  struct spinlock lock;
  int count;
};

void V(struct semaphore *s)
{
  acquire(&s->lock);
  s->count += 1;
  release(&s->lock);
}

void P(struct semaphore *s)
{
  while (s->count == 0)
    ;
  acquire(&s->lock);
  s->count -= 1;
  release(&s->lock);
}
```
+ 上面的实现是代价很大
  + 如果生产者很少生产，消费者将把大部分时间花在while循环中，希望得到一个非零的计数
  + 消费者的CPU可以通过反复轮询(polling) s->count可以找到比忙碌等待(busy waiting)更有效的工作
  + 避免忙碌等待需要一种方法，让消费者让出CPU，只有在V增加计数后才恢复
+ 这里是朝着这个方向迈出的一步，虽然他不能完全解决这个问题
+ 让我们想象一对调用，sleep和wakeup，其工作原理如下:
  + Sleep(chan)睡眠chan上，chan可以为任意值，称为等待通道(wait channel)
  + Sleep使调用进程进入睡眠状态，释放CPU进行其他工作
  + Wakeup(chan)唤醒所有在chan上sleep的进程（如果有的话），使它们的sleep调用返回
  + 如果没有进程在chan上等待，则wakeup不做任何事情
  + 我们修改信号量实现，以使用sleep和wakeup（修改处用注释标注）
```c
void V(struct semaphore *s)
{
  acquire(&s->lock);
  s->count += 1;
  wakeup(s);			// 修改
  release(&s->lock);
}

void P(struct semaphore *s)
{
  while (s->count == 0)
    sleep(s);			// 修改
  acquire(&s->lock);
  s->count -= 1;
  release(&s->lock);
}
```
+ P现在放弃CPU而不是自旋，这是一个不错的改进
+ 然而，事实证明，像这样设计sleep和wakeup并不是一件容易的事
  + 因为它会遇到所谓的丢失唤醒问题
  + 假设执行P 的s->count == 0这一行时
  + 当P在sleep之前，V在另一个CPU上运行：
    + 它将s->count改为非零，并调用wakeup
    + wakeup发现没有进程在睡眠，因此什么也不做
  + 现在P继续执行：
    + 它调用sleep并进入睡眠状态
  + 这就造成了一个问题：
    + P正在sleep，等待一个已经发生的V调用
    + 除非我们运气好，生产者再次调用V，否则消费者将永远等待，即使计数是非零
+ 这个问题的根源在于，在错误的时刻运行的V违反了P只在s->count==0时休眠的不变量
+ 保护这个不变量的一个不正确的方法是将锁获取（修改用注释标注）移动到P中，这样它对计数的检查和对sleep的调用是原子的：
```c
void
V(struct semaphore *s)
{
	acquire(&s->lock);
	s->count += 1;
	wakeup(s);
	release(&s->lock);
}

void
P(struct semaphore *s)
{
	acquire(&s->lock);		// 修改
	while(s->count == 0)
		sleep(s);
	s->count -= 1;
	release(&s->lock);
}
```
+ 人们可能希望这个版本的P能够避免丢失的唤醒，因为锁会阻止V在s->count == 0和sleep之间执行
+ 它做到了这一点，但它也会死锁。P在sleep时保持着锁，所以V将永远阻塞在等待锁的过程中
+ 我们将通过改变sleep的接口来修正前面的方案：
  + 调用者必须将条件锁(condition lock)传递给sleep
  + 这样在调用进程被标记为SLEEPING并在chan上等待后，它就可以释放锁
  + 锁将强制并发的V等待直到P将自己置于SLEEPING状态
  + 这样wakeup就会发现SLEEPING的消费者并将其唤醒
  + 一旦消费者再次被唤醒，sleep就会重新获得锁，然后再返回
+ 我们新的正确的睡眠/唤醒方案是可用的，如下所示（修改用注释标注）
```c
void
P(struct semaphore *s)
{
	acquire(&s->lock);
	while(s->count == 0)
		sleep(s, &s->lock);	// 修改
	s->count -= 1;
	release(&s->lock);
}
```
+ P持有s->lock会阻止了V在P检查c->count和调用sleep之间试图唤醒它
+ **我们需要sleep来原子地释放s->lock并使消费者进程进入SLEEPING状态**
### Code: Sleep and wakeup
+ sleep (kernel/proc.c:548) 和 wakeup (kernel/proc.c:582) 的实现
  + 其基本思想是让sleep将当前进程标记为SLEEPING，然后调用sched让出CPU
  + wakeup则寻找给定的等待通道上睡眠的进程，并将其标记为RUNNABLE
  + sleep和wakeup的调用者可以使用任何方便的数字作为channel
  + Xv6经常使用参与等待的内核数据结构的地址
+ Sleep首先获取p->lock (kernel/proc.c:559)
  + 现在进入睡眠状态的进程同时持有p->lock和lk
  + 在调用者(在本例中为P)中，持有lk是必要的：
    + 它保证了没有其他进程(在本例中，运行V的进程)可以调用wakeup(chan)
+ 现在sleep持有p->lock，释放lk是安全的：
  + 其他进程可能会调用wakeup(chan)，但wakeup会等待获得p->lock
  + 因此会等到sleep将进程状态设置为SLEEPING，使wakeup不会错过sleep的进程
+ 有一个复杂情况：
  + 如果lk和p->lock是同一个锁
  + 如果sleep仍试图获取p->lock，就会和自己死锁
  + 但是如果调用sleep的进程已经持有p->lock，那么它就不需要再做任何事情来避免错过一个并发的wakeup
    + 这样的情况发生在，wait (kernel/proc.c:582)调用sleep并持有p->lock时
+ 现在sleep持有p->lock，而没有其他的锁
  + 它可以通过记录它睡眠的channel，将进程状态设置SLEEPING
  + 并调用sched(kernel/proc.c:564-567)来使进程进入睡眠状态
  + 稍后我们就会明白为什么在进程被标记为SLEEPING之前，p->lock不会被释放（由调度器）
+ 在某些时候，一个进程将获取条件锁，设置睡眠等待的条件，并调用wakeup(chan)
+ 重要的是，wakeup是在持有条件锁[2]的情况下被调用的
  + Wakeup循环浏览进程表（kernel/proc.c:582）。它获取每个被检查的进程的p->lock
  + 因为它可能会修改该进程的状态，也因为p->sleep确保sleep和wakeup不会相互错过
  + 当wakeup发现一个进程处于状态为SLEEPING并有一个匹配的chan时，它就会将该进程的状态改为RUNNABLE
  + 下一次调度器运行时，就会看到这个进程已经准备好运行了
+ 为什么sleep和wakeup的锁规则能保证睡眠的进程不会错过wakeup？
  + sleep进程从检查条件之前到标记为SLEEPING之后的这段时间里，持有条件锁或它自己的p->lock或两者都持有
  + 调用wakeup的进程在wakeup的循环中持有这两个锁
  + 因此，唤醒者要么在消费者检查条件之前使条件为真
  + 要么唤醒者的wakeup在消费者被标记为SLEEPING之后检查它
  + 无论怎样，wakeup就会看到这个睡眠的进程，并将其唤醒（除非有其他事情先将其唤醒）
+ 有时会出现多个进程在同一个channel上睡眠的情况
  + 例如，有多个进程从管道中读取数据
  + 调用一次wakeup就会把它们全部唤醒
  + 其中一个进程将首先运行，并获得sleep参数传递的锁，（就管道而言）读取数据都会在管道中等待
  + 其他进程会发现，尽管被唤醒了，但没有数据可读
  + 从他们的角度来看，唤醒是“虚假的“，他们必须再次睡眠
  + 出于这个原因，sleep总是在一个检查条件的循环中被调用
+ 如果两次使用sleep/wakeup不小心选择了同一个通道，也不会有害：
  + 它们会看到虚假的唤醒，上面提到的循环允许发生这种情况
  + sleep/wakeup的魅力很大程度上在于它既是轻量级的（不需要创建特殊的数据结构来充当睡眠通道）
  + 又提供了一层间接性（调用者不需要知道他们正在与哪个具体的进程交互）

### Code: Pipes







































































































































