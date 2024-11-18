# 处理所有中断的代码
```c
struct spinlock tickslock;  // 定义一个自旋锁，用于保护全局变量ticks
uint ticks;                 // 定义一个全局变量ticks，记录时间滴答数

// 声明外部变量，指向汇编代码中的 trampolines和用户模式的向量表
extern char trampoline[], uservec[], userret[];

// 在 kernelvec.S 文件中定义，该函数调用 kerneltrap()，用于调用内核陷进处理函数.
void kernelvec();

// 处理设备中断
extern int devintr();

// 初始化陷进处理
void
trapinit(void)
{
// 初始化自旋锁tickslock，用于保护时间滴答数
  initlock(&tickslock, "time");
}

// 设置当前处理器以处理内核模式下的异常和陷进
void
trapinithart(void)
{
    // 设置stves寄存器，指向内核向量表kernelvec
  w_stvec((uint64)kernelvec);
}

//
// 处理来自用户空间的中断、异常或系统调用
// 该函数从 trampoline.S 文件中调用
//
void
usertrap(void)
{
  int which_dev = 0;

  // 检查是否来自用户模式，如果不是则出发panic()
  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // 设置 stvec 寄存器，指向内核陷阱向量表 kernelvec
  // 因为现在已经进入内核模式.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();  // 获取当前进程
  
  // 保存用户程序计数器（用户态 PC）
  p->trapframe->epc = r_sepc();
  
  // 检查 scause 寄存器的值，判断是否是系统调用
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc 指向 ecall 指令，但我们希望返回到下一条指令.
    p->trapframe->epc += 4;

    // 中断将改变 sstatus 等寄存器
    // 所以在完成这些寄存器的操作之前不要启用中断
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // 处理设备中断（ok）
  } else {
    // 如果不是系统调用也不是设备中断，打印错误信息并标记进程为 killed
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  // 如果进程已被标记为killed，退出并返回-1
  if(p->killed)
    exit(-1);

  // 如果是定时器中断，放弃 CPU
  if(which_dev == 2)
    yield();

  // 返回到用户模式
  usertrapret();
}

//
// 返回用户空间
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // 我们即将把陷阱的目的地从 kerneltrap() 切换到 usertrap()
  // 因此在我们回到用户空间之前关闭中断
  // 在用户空间usertrap()才是正确的处理程序
  intr_off();

  // 将系统调用、中断和异常发送到 trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // 设置 trapframe 值，uservec 在进程下次进入内核时将需要这些值
  p->trapframe->kernel_satp = r_satp();         // 内核页表
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // 进程的内核栈
  p->trapframe->kernel_trap = (uint64)usertrap; // 用户态陷进处理函数
  p->trapframe->kernel_hartid = r_tp();         // hartid 用于获取 cpuid()

  // 设置 trampoline.S 的 sret 将用来进入用户空间的寄存器值
  
  // 设置S 先前特权模式为用户模式.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // 将 SPP 清零以切换到用户模式
  x |= SSTATUS_SPIE; // 启用用户模式下的中断
  w_sstatus(x);

  // 设置 S 异常程序计数器为保存的用户程序计数器.
  w_sepc(p->trapframe->epc);

  // 告诉 trampoline.S 要切换到的用户页表.
  uint64 satp = MAKE_SATP(p->pagetable);

// 跳转到内存顶端的 trampoline.S
// 它将切换到用户页表，恢复用户寄存器
// 并使用 sret 指令切换到用户模式
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// 中断和异常从内核代码通过 kernelvec 进入这里，在当前的内核栈上进行处理
void 
kerneltrap()
{
  int which_dev = 0;            // 变量用于识别设备中断类型
  uint64 sepc = r_sepc();       // 保存当前的异常程序计数器（EPC）
  uint64 sstatus = r_sstatus(); // 保存当前的状态当前寄存器（SSTATUS）
  uint64 scause = r_scause();   // 保存当前的异常原因（SCAUSE）
  
  // 检查是否是从超级模式(supervisor mode)进入的中断处理
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  
  // 检查中断是否启用，若启用则触发 panic
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  // 调用设备中断处理程序，如果返回 0，表示不是设备中断
  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);  // 输出异常原因
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());  // 输出EPC 和 STVAL
    panic("kerneltrap");
  }

  // 如果是定时器中断且当前进程正在运行，则放弃 CPU.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // yield() 可能会导致一些陷阱发生
  // 因此需要恢复陷阱寄存器，以供 kernelvec.S 的 sepc 指令使用.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

// 定时器中断处理程序
void
clockintr()
{
  acquire(&tickslock);  // 获取ticks自旋锁
  ticks++;              // 增加全局时间滴答计数
  wakeup(&ticks);       // 唤醒等待时间滴答的进程
  release(&tickslock);  // 释放ticks自旋锁
}

// 检查是否是外部中断或软件中断，并处理它们
// 如果是定时器中断，返回 2 
// 如果是其他设备中断，返回 1 
// 如果未识别，返回 0
int
devintr()
{
  uint64 scause = r_scause(); // 获取当前的异常原因

  // 检查是否来自PILC的超级外部中断
  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // 这是通过PLIC的超级外部中断.

    // irq 指示哪个设备产生了中断.
    int irq = plic_claim();

    // 检查中断源并调用相应的中断处理函数
    if(irq == UART0_IRQ){
      uartintr();   // 处理UART0中断
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr(); // 处理Virtio 磁盘中断
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq); // 处理意外的中断
    }

    // PLIC 允许每个设备一次产生最多一个中断
    // 通知 PLIC 该设备现在可以再次中断
    if(irq)
      plic_complete(irq);

    return 1;// 表示其他设备中断
  } else if(scause == 0x8000000000000001L){
    // 检查是否是来自机器模式定时器的超级软件中断
    // 该中断通过 kernelvec.S 中的 timervec 转发.

    if(cpuid() == 0){
      clockintr();  // 处理定时器中断
    }
    
    // 通过清除 sip 寄存器中的 SSIP 位来确认软件中断
    w_sip(r_sip() & ~2);

    return 2; // 表示这是一个定时器中断
  } else {
    return 0; // 未识别的中断
  }
}
```