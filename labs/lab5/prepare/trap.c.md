```c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// 在 kernelvec.S 中，调用 kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// 设置为在内核中处理异常和陷阱.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// 处理来自用户空间的中断、异常或系统调用
// 从 trampoline 调用。S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

// 将中断和异常发送到 kerneltrap()，
// 因为我们现在在内核中.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  //保存用户程序计数器.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc 指向 ecall 指令，但我们想返回下一条指令.
    p->trapframe->epc += 4;

    // 中断将改变 sstatus 和 c 寄存器，因此在完成这些寄存器的操作之前不要启用它
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
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

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // 我们即将把陷阱的目标从 kerneltrap() 切换到 usertrap()
  // 因此关闭中断，直到我们回到用户空间，其中 usertrap() 是正确的。
  intr_off();

  // 将系统调用、中断和异常发送到 trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // 设置进程下次重新进入内核时 uservec 需要的 trapframe 值
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid 用于 cpuid()

  // 设置 trampoline.S 的 sret 将用于进入用户空间的寄存器.
  
  // 将 S 先前的特权模式设置为用户.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // 清除 SPP 为 0 以进入用户模式
  x |= SSTATUS_SPIE; // 在用户模式下启用中断
  w_sstatus(x);

  // 将 S 异常程序计数器设置为保存的用户 PC.
  w_sepc(p->trapframe->epc);

  // 告诉 trampoline.S 要切换到的用户页表.
  uint64 satp = MAKE_SATP(p->pagetable);

  // 跳转到内存顶部的 trampoline.S，
  // 切换到用户页表，恢复用户寄存器，并使用 sret 切换到用户模式.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// 内核代码中的中断和异常通过 kernelvec 传递到这里，无论当前内核堆栈是什么.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // 如果这是定时器中断，则放弃 CPU.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // Yield() 可能导致某些陷阱发生，因此恢复陷阱寄存器以供 kernelvec.S 的 sepc 指令使用.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// 检查它是否是外部中断或软件中断，并处理它
// 如果是定时器中断则返回 2
// 如果是其他设备则返回 1
// 如果无法识别则返回 0.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // 这是一个监控外部中断，通过 PLIC。 
 
    // irq 指示哪个设备中断.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // PLIC 允许每个设备一次最多发出一个中断；告诉 PLIC 该设备现在被允许再次中断.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // 来自机器模式定时器中断的软件中断，由 kernelvec.S 中的 timervec 转发

    if(cpuid() == 0){
      clockintr();
    }
    
    //通过清除 sip 中的 SSIP 位来确认软件中断.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}


```