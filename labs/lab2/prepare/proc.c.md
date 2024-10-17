```c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void wakeup1(struct proc *chan);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// 在启动时初始化 proc 表
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");

// 为进程的内核堆栈分配一个页面
// 将其映射到内存的高位，后跟无效的保护页面
      char *pa = kalloc();
      if(pa == 0)
        panic("kalloc");
      uint64 va = KSTACK((int) (p - proc));
      kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
      p->kstack = va;
  }
  kvminithart();
}

// 必须在禁用中断的情况下调用，以防止与被移动,到另一个 CPU 的进程发生竞争
int
cpuid()
{
  int id = r_tp();
  return id;
}

// 返回此 CPU 的 cpu 结构
// 必须禁用中断
struct cpu*
mycpu(void) {
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// 返回当前 struct proc *，如果没有则返回零
struct proc*
myproc(void) {
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid() {
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// 在进程表中查找未使用的进程
//     如果找到，则初始化内核中运行所需的状态，并返回 p->lock 持有的状态
//     如果没有空闲的进程，或者内存分配失败，则返回 0
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();

  // 分配一个 trapframe 页面.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }

// 一个空的用户页表.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

// 设置新的上下文以开始在 forkret 处执行，
// 它返回到用户空间
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// 释放 proc 结构以及其中悬挂的数据，包括用户页面。
// 必须保持 p->lock
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// 为给定进程创建用户页表，没有用户内存，但有trampoline页。
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

// 将 trampoline 代码（用于系统调用返回）映射到最高的用户虚拟地址
// 只有管理员使用它，在往返于用户空间的途中，所以不是 PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

// 将 trapframe 映射到 TRAMPOLINE 正下方，用于 trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// 释放进程的页表，并释放其引用的 
// 物理内存
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// 调用 exec("/init") 的用户程序 
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// 设置第一个用户进程.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
// 分配一个用户页面并将 init 的指令 
// 和数据复制到其中
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // 为从内核到用户的第一次“return”做准备.
  p->trapframe->epc = 0;      // 用户程序计数器
  p->trapframe->sp = PGSIZE;  // 用户堆栈指针

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// 将用户内存增加或减少 n 字节 
// 成功时返回 0，失败时返回 -1
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// 创建一个新进程，复制父进程。
// 设置子内核堆栈以返回，就像从 fork() 系统调用返回
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

// 将 p 被遗弃的子代传递给 init
// 调用者必须持有 p->lock
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
// 此代码使用 pp->parent 而不持有 pp->lock 
    // 首先获取锁可能会导致死锁 
    // 如果 pp 或 pp 的子级也在 exit() 
    // 并且即将尝试锁定 p
    if(pp->parent == p){
  // pp->parent 不能在 check 和 acquire() 之间改变 
  // 因为只有父级会改变它，而我们就是父级
      acquire(&pp->lock);
      pp->parent = initproc;
  // 我们应该在这里唤醒 init，但这需要 initproc->lock，这会导致死锁
  // 因为我们持有 init 的一个子进程（pp）上的锁
  // 这就是为什么 exit() 总是唤醒 init（在获取任何锁之前）
      release(&pp->lock);
    }
  }
}

// 退出当前进程。不返回。
// 退出的进程保持僵尸状态 
// 直到其父进程调用 wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

// 我们可能会将一个子进程重新设置为 init 的父进程。我们无法精确地唤醒 init，因为一旦我们 
 // 获得了任何其他 proc 锁，我们就无法获取它的锁。因此，无论是否有必要，都请唤醒 init。init 可能会错过这次唤醒，但这似乎是无害的
  acquire(&initproc->lock);
  wakeup1(initproc);
  release(&initproc->lock);

// 获取 p->parent 的副本，以确保我们解锁我们锁定的相同父级
// 以防我们的父级在等待父级锁定时，将我们交给 init
// 然后我们可能会与退出的父级竞争，但结果将是无害的虚假唤醒到死亡或错误的进程
// proc 结构永远不会像其他任何东西一样重新分配 
  acquire(&p->lock);
  struct proc *original_parent = p->parent;
  release(&p->lock);
  
  // we need the parent's lock in order to wake it up from wait().
  // the parent-then-child rule says we have to lock it first.
  acquire(&original_parent->lock);

  acquire(&p->lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup1(original_parent);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&original_parent->lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// 等待子进程退出并返回其 pid
// 如果此进程没有子进程，则返回 -1
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

// 一直保持 p->lock 以避免丢失 
// 从子进程退出时唤醒()
  acquire(&p->lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      // this code uses np->parent without holding np->lock.
      // acquiring the lock first would cause a deadlock,
      // since np might be an ancestor, and we already hold p->lock.
      if(np->parent == p){
        // np->parent can't change between the check and the acquire()
        // because only the parent changes it, and we're the parent.
        acquire(&np->lock);
        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&p->lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&p->lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&p->lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &p->lock);  //DOC: wait-sleep
  }
}

// 每个 CPU 的进程调度程序
// 每个 CPU 在设置自身后都会调用 scheduler()。
// 调度程序永远不会返回。它会循环执行：
// - 选择要运行的进程。
// - swtch 开始运行该进程。
// - 最终该进程通过 swtch 将控制权转移回调度程序
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();
    
    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;

        found = 1;
      }
      release(&p->lock);
    }
    if(found == 0) {
      intr_on();
      asm volatile("wfi");
    }
  }
}

// 切换到调度程序
// 必须只保留 p->lock，并已更改 proc->state 保存并恢复intena
// 因为 intena 是此内核线程的属性，而不是此 CPU
// 它应该是proc->intena 和 proc->noff，但这会在持有锁但没有进程的少数地方中断 
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// 放弃一个调度轮次的 CPU
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// fork 子进程第一次由 scheduler() 调度 
// 将切换到 forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.
  if(lk != &p->lock){  //DOC: sleeplock0
    acquire(&p->lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &p->lock){
    release(&p->lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
    }
    release(&p->lock);
  }
}

// Wake up p if it is sleeping in wait(); used by exit().
// Caller must hold p->lock.
static void
wakeup1(struct proc *p)
{
  if(!holding(&p->lock))
    panic("wakeup1");
  if(p->chan == p && p->state == SLEEPING) {
    p->state = RUNNABLE;
  }
}

// 终止具有给定 pid 的进程
// 受害者不会退出，直到它尝试将 
// 返回到用户空间（参见 trap.c 中的 usertrap()）
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// 复制到用户地址或内核地址，
// 取决于 usr_dst
// 成功时返回 0，错误时返回 -1
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// 从用户地址或内核地址复制
// 取决于 usr_src 成功时返回 0，错误时返回 -1
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// 将进程列表打印到控制台，用于调试。
// 当用户在控制台上键入 ^P 时运行。
// 没有锁定，以避免进一步卡住机器.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}


// 统计非 UNUSED 状态的进程数量
uint64
count_process(void){
  uint64 count = 0;
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state!= UNUSED){
      count++;
    }
  }
  return count;
}
```