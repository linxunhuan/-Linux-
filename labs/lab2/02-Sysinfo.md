## 题目
+ 在此作业中，您将添加一个系统调用 sysinfo，用于收集有关正在运行的系统的信息
+ 该系统调用需要一个参数：
  + 指向 struct sysinfo 的指针（请参阅 kernel/sysinfo.h）
+ 内核应填写此结构中的字段：
  + freemem 字段应设置为可用内存的字节数
  + nproc 字段应设置为状态不是 UNUSED 的进程数
+ 我们提供了一个测试程序 sysinfotest
  + 如果它打印“sysinfotest：OK”，则您通过了此作业
## 官方提示
+ 在 Makefile 中的 UPROGS 中 添加$U/_sysinfotest
+ 运行make qemu;user/sysinfotest.c将无法编译
  + 添加系统调用 sysinfo，步骤与上一个作业相同
  + 要在 user/user.h 中声明 sysinfo() 的原型，您需要预先声明struct sysinfo的存在：

    struct sysinfo；
    int sysinfo（struct sysinfo *）；
    
     一旦您解决了编译问题，运行 sysinfotest.它将失败，因为您尚未在内核中实现系统调用
+ sysinfo 需要将struct sysinfo复制回用户空间；
+ 有关如何使用copyout()执行此操作的示例
  + 请参阅sys_fstat() ( kernel/sysfile.c ) 
  + filestat() ( kernel/file.c ) 
+ 为了收集可用内存量，在kernel/kalloc.c中添加一个函数
+ 为了收集进程数，在kernel/proc.c中添加一个函数

## 做题思路
+ 由于页表不同，指针不能直接互通访问
  + 也就是内核不能直接对用户态传进来的指针进行解引用
  + 需要使用 copyin、copyout 方法结合进程的页表，才能顺利找到用户态指针（逻辑地址）对应的物理内存地址
+ 获取空闲内存
  + xv6采用空闲链表法来记录空闲页
  + 就是把空闲内存页本身作为链表节点，形成一个空闲页链表（由kmem.freelist指向）
  + 需要分配空闲页时，直接返回kmem.freelist所指向的空闲内存页
  + 需要回收时，用tmp指针指向kmem.freelist所指向的空闲内存页
    + 然后kmem.freelist指向tmp->next
    + 作为新的空闲内存页的链表头
+ 表示进程的不同状态
  + UNUSED：
    + 表示进程控制块（PCB）未被使用
    + 这个状态通常用于表示空闲的 PCB，可以被分配给新的进程
  + SLEEPING：
    + 表示进程正在等待某个事件（如 I/O 操作完成）
    + 在这个状态下，进程不会被调度执行
  + RUNNABLE：
    + 表示进程已经准备好运行，可以被调度器选择执行
    + 这个状态的进程在就绪队列中等待 CPU 时间片
  + RUNNING：
    + 表示进程当前正在 CPU 上执行
    + 只有一个进程可以处于这个状态（在单核 CPU 上）
  + ZOMBIE：
    + 表示进程已经终止，但其父进程尚未调用 wait() 系统调用获取其终止状态
    + 僵尸进程仍然保留在系统中，直到其父进程处理


## 做题步骤

### 获取空闲内存
+ 在内核的头文件中声明计算空闲内存的函数
  + 经通篇看下来，这个文件内容是内存相关的，所以放在 kalloc、kfree 等函数的的声明之后
  + 不要头铁，声明函数只能是uint64，不要学我一开始写uint16
<img src=".\picture\image10.png">

 + 在 kalloc.c 中添加计算空闲内存的函数：
```c
uint64
count_free_mem(void){

  // 统计空闲页数，加上页大小PGSIZE 就是空闲的内存字节数
  uint64 free_mem = 0;
  struct run *r = kmem.freelist;
  while(r!= NULL){
    free_mem += PGSIZE;
    r = r->next;
  }
 
  return free_mem;
}
```
+ 多插一句：
  + 由于是直接使用空闲页本身作为链表节点
  + 所以不需要使用额外空间来存储空闲页链表
  + 所以在 kalloc() 里也可以看到
    + 分配内存的最后一个阶段，是直接将 freelist 的根节点地址（物理地址）返回出去了：
```c
// 分配一个 4096 字节的物理内存页 
// 返回内核可以使用的指针
// 如果无法分配内存，则返回 0
void *
kalloc(void)
{
  // 定义一个指向空闲内存块的指针 r
  struct run *r;

  acquire(&kmem.lock);
  
  // 将空闲内存列表的头指针赋值给 r，即取出第一个空闲内存块
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```

### 获取运行的进程数

+ 在内核的头文件中添加函数声明：
  + 这次写这里的原因，也只是大部分的执行功能都放这里了
<img src=".\picture\image11.png">

+ 在 proc.c 中实现该函数：
```c
// 统计非 UNUSED 状态的进程数量
// 不是 UNUSED 的进程位，就是已经分配的
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

## 实现 sysinfo 系统调用
+ 在MakeFile中添加sysinfotest
+ 在kernel/syscall.h添加新的system call的序号
+ 用extern全局声明新的内核调用函数，并且在syscalls映射表中，加入之前定义的编号–>系统调用函数指针的映射
+ 在user/usys.pl中添加从用户态到内核态的跳板函数
+ 由于在kernel/sysproc.c中我们会用到sysinfo结构，其定义在sysinfo.h，所以我们要在kernel/sysproc.c中引入这个库
<img src=".\picture\image13.png">

+ 在kernel/sysproc.c中编写sys_sysinfo函数
```c
uint64
sys_sysinfo(void){
  // 从用户态读入一个指针，作为存放sysinfo的buffer
  uint64 addr;
  if(argaddr(0, &addr) < 0){
    return -1;
  }

  // 定义一个sysinfo结构的变量sinfo，记录系统调用的信息
  struct sysinfo sinfo;
  sinfo.freemem = count_free_mem();//统计空闲内存字节数
  sinfo.nproc = count_process();     //统计已创建的进程数

  // 复制sinfo的内容到用户态传来的地址
  // 使用copyout，结合当前进程的页表，获得进程传来的指针（逻辑地址）对应的物理地址
  // 然后将 $sinfo 中的数据复制到该指针所指向的位置，供用户进程使用
  if (copyout(myproc()->pagetable,addr, (char*)&sinfo,sizeof(sinfo)) < 0){
    return -1;
  } 
  return 0;
}
```

## 编译结果
下面是我的运行结果，符合题目要求
<img src=".\picture\image14.png">