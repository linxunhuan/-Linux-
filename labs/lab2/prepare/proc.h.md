```C
// 用于内核上下文切换的保存的寄存器
struct context {
  uint64 ra;
  uint64 sp;

  // 被调用者保存
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

// Per-CPU state.
struct cpu {
  struct proc *proc;          // 该 CPU 上运行的进程，或者为 null
  struct context context;     // swtch() 此处进入 scheduler()
  int noff;                   // push_off() 嵌套的深度.
  int intena;                 // 在 push_off() 之前是否启用了中断
};

extern struct cpu cpus[NCPU];

// trampoline.S 中陷阱处理代码的每个进程数据 

// 位于用户页表中 trampoline 页下方的一个页面中。未在内核页表中特别映射

// sscratch 寄存器指向此处

// trampoline.S 中的 uservec 将用户寄存器保存在 trapframe 中，然后从 trapframe 的 kernel_sp、kernel_hartid、kernel_satp 初始化寄存器，并跳转到 kernel_trap

// trampoline.S 中的 usertrapret() 和 userret 设置 trapframe 的 kernel_*，从trapframe 恢复用户寄存器，切换到用户页表，然后进入用户空间

// trapframe 包括被调用方保存的用户寄存器，如 s0-s11，因为通过 usertrapret() 的返回用户路径不会通过整个内核调用堆栈返回
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // 内核页表
  /*   8 */ uint64 kernel_sp;     // 进程内核栈的顶部
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // 保存的用户程序计数器
  /*  32 */ uint64 kernel_hartid; // 保存的内核 tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// 每个进程的状态
struct proc {
  struct spinlock lock;

  // 使用这些时必须持有 p->lock：
  enum procstate state;        // 进程状态
  void *chan;                  // 如果不为零，则在 chan 上睡觉
  int killed;                  // 若非零，则已被杀死
  int xstate;                  // 退出状态返回给父进程等待
  int pid;                     // Process ID

  // 使用此项时必须持有 wait_lock：
  struct proc *parent;         // Parent process

  // 这些对于进程来说是私有的，因此不需要持有 p->lock
  uint64 kstack;               // 内核堆栈的虚拟地址
  uint64 sz;                   // 进程内存的大小（字节）
  pagetable_t pagetable;       // 用户页表
  struct trapframe *trapframe; // trampoline.S 的数据页面
  struct context context;      // swtch() 此处运行进程
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // 进程名称（调试）
};
```