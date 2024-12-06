```c
/* 线程的可能状态： */
#define FREE        0x0  // 空闲
#define RUNNING     0x1  // 运行
#define RUNNABLE    0x2  // 可运行，即等待调度

#define STACK_SIZE  8192 // 每个线程的栈大小（字节）
#define MAX_THREAD  4    // 最大线程数

/* 线程结构体，表示一个线程的信息 */
struct thread {
  char       stack[STACK_SIZE];  // 线程的栈空间
  int        state;              // 线程的状态，可能的状态有 FREE, RUNNING, RUNNABLE
  struct {
    uint64 sp, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11; // 保存线程的寄存器状态
    uint64 ra; // 返回地址寄存器，表示线程从何处返回
  } reg_state; // 线程的寄存器状态（保存寄存器的值）
};

/* 定义一个包含 MAX_THREAD 个线程的数组，用于存储所有线程 */
struct thread all_thread[MAX_THREAD];

/* 指向当前正在运行线程的指针 */
struct thread *current_thread;

/* 声明上下文切换函数，供其他地方调用 */
extern void thread_switch(uint64, uint64);

/* 线程初始化函数 */
void thread_init(void)
{
  // main() 是线程 0，该线程将第一次调用 thread_schedule() 函数
  // 它需要一个栈空间，以便第一次执行 thread_switch() 时保存线程 0 的状态
  // 线程调度程序（thread_schedule）不会再运行主线程，因为主线程的状态被设置为 RUNNING，
  // 而线程调度程序只会选择处于 RUNNABLE 状态的线程来运行
  
  current_thread = &all_thread[0];  // 设置当前线程为线程 0（主线程）
  current_thread->state = RUNNING;  // 设置线程 0 为正在运行状态
}

/* 线程调度函数 */
void thread_schedule(void)
{
  struct thread *t, *next_thread;

  /* 寻找下一个可运行的线程 */
  next_thread = 0;  // 初始化为 NULL，表示目前还没有找到可运行线程
  t = current_thread + 1;  // 从当前线程的下一个线程开始查找

  /* 遍历所有线程，查找第一个 RUNNABLE 状态的线程 */
  for (int i = 0; i < MAX_THREAD; i++) {
    // 如果当前线程指针越界，则重置为 all_thread 的第一个线程
    if (t >= all_thread + MAX_THREAD)
      t = all_thread;

    // 如果找到 RUNNABLE 状态的线程
    if (t->state == RUNNABLE) {
      next_thread = t;  // 将该线程设为下一个要运行的线程
      break;  
    }

    t = t + 1;  // 否则，指针指向下一个线程
  }

  /* 如果没有找到可运行的线程 */
  if (next_thread == 0) {
    printf("thread_schedule: no runnable threads\n");
    exit(-1); 
  }

  /* 如果当前线程与下一个线程不同，则进行线程切换 */
  if (current_thread != next_thread) {
    next_thread->state = RUNNING;  // 设置下一个线程的状态为 RUNNING（正在运行）

    t = current_thread;  // 保存当前线程的指针
    current_thread = next_thread;  // 切换当前线程为下一个线程

    /* 执行上下文切换 */
    // 调用 thread_switch 函数，进行当前线程与下一个线程之间的上下文切换
    // 将当前线程的寄存器状态（reg_state）保存，并加载下一个线程的寄存器状态
    thread_switch((uint64)&t->reg_state, (uint64)&current_thread->reg_state);
  } else {
    // 如果当前线程就是下一个要运行的线程，则不需要切换
    next_thread = 0;
  }
}

/* 线程创建函数 */
void thread_create(void (*func)()) 
{
  struct thread *t;

  // 遍历所有线程，寻找一个处于 FREE 状态的线程
  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break; 
  }

  t->state = RUNNABLE;  

  /* 设置线程的寄存器状态 */
  // 设置线程的返回地址寄存器 (ra)，即线程函数的入口地址
  t->reg_state.ra = (uint64)func;

  // 设置线程的栈指针 (sp)，初始化为线程栈的顶部
  t->reg_state.sp = (uint64)(t->stack + STACK_SIZE);  
}

/* 线程让渡函数 */
void thread_yield(void)
{
  current_thread->state = RUNNABLE; 
  thread_schedule();  // 调用线程调度函数，尝试切换到下一个可运行的线程
}

// 用于表示线程 a、b、c 是否已经启动的标志变量
volatile int a_started, b_started, c_started;  

// 用于记录每个线程处理的循环次数
volatile int a_n, b_n, c_n;  

/* 线程 a 的执行函数 */
void thread_a(void)
{
  int i;
  printf("thread_a started\n");  // 打印线程 a 启动的消息
  a_started = 1;  // 设置 a_started 为 1，表示线程 a 已经启动
  // 等待线程 b 和 c 都启动后再开始执行
  while (b_started == 0 || c_started == 0)
    thread_yield();  // 如果 b 或 c 还没有启动，则让出 CPU

  // 线程 a 执行主循环，打印信息并增加 a_n 计数
  for (i = 0; i < 100; i++) {
    printf("thread_a %d\n", i);  // 打印线程 a 的迭代次数
    a_n += 1;  // 增加线程 a 计数
    thread_yield();  // 让出 CPU，允许其他线程执行
  }
  
  printf("thread_a: exit after %d\n", a_n);  // 打印线程 a 执行结束时的计数

  current_thread->state = FREE;  
  thread_schedule();  // 调用线程调度函数，切换到下一个可运行的线程
}

/* 线程 b 的执行函数 */
void thread_b(void)
{
  int i;
  printf("thread_b started\n");  // 打印线程 b 启动的消息
  b_started = 1;  // 设置 b_started 为 1，表示线程 b 已经启动
  // 等待线程 a 和 c 都启动后再开始执行
  while (a_started == 0 || c_started == 0)
    thread_yield();  // 如果 a 或 c 还没有启动，则让出 CPU

  // 线程 b 执行主循环，打印信息并增加 b_n 计数
  for (i = 0; i < 100; i++) {
    printf("thread_b %d\n", i);  // 打印线程 b 的迭代次数
    b_n += 1;  // 增加线程 b 计数
    thread_yield();  // 让出 CPU，允许其他线程执行
  }

  printf("thread_b: exit after %d\n", b_n);  // 打印线程 b 执行结束时的计数

  current_thread->state = FREE;  
  thread_schedule(); 
}

/* 线程 c 的执行函数 */
void thread_c(void)
{
  int i;
  printf("thread_c started\n");  // 打印线程 c 启动的消息
  c_started = 1;  // 设置 c_started 为 1，表示线程 c 已经启动
  // 等待线程 a 和 b 都启动后再开始执行
  while (a_started == 0 || b_started == 0)
    thread_yield();  // 如果 a 或 b 还没有启动，则让出 CPU

  // 线程 c 执行主循环，打印信息并增加 c_n 计数
  for (i = 0; i < 100; i++) {
    printf("thread_c %d\n", i);  // 打印线程 c 的迭代次数
    c_n += 1;  // 增加线程 c 计数
    thread_yield();  // 让出 CPU，允许其他线程执行
  }

  printf("thread_c: exit after %d\n", c_n);  // 打印线程 c 执行结束时的计数

  current_thread->state = FREE;  
  thread_schedule();  
}

/* 主函数，初始化线程并启动线程调度 */
int main(int argc, char *argv[]) 
{
  a_started = b_started = c_started = 0;  // 初始化 a、b、c 启动标志为 0，表示线程尚未启动
  a_n = b_n = c_n = 0;  // 初始化 a、b、c 的计数为 0

  thread_init();  // 初始化线程系统（创建线程等）

  // 创建线程 a、b 和 c，将它们的执行函数传递给线程创建函数
  thread_create(thread_a);  
  thread_create(thread_b);  
  thread_create(thread_c);  

  thread_schedule();  // 启动线程调度，开始运行线程
  exit(0); 
}
```