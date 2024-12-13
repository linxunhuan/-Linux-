# 锁
## 官方介绍
+ 在本实验中，您将获得重新设计代码以提高并行性的经验
+ 多核机器上并行性差的一个常见症状是锁争用率高
+ 提高并行性通常需要更改数据结构和锁定策略以减少争用
+ 您将针对 xv6 内存分配器和块缓存执行此操作
+ 在编写代码之前，请务必阅读xv6 书中的以下部分：
  + 第六章：“锁定”以及相应的代码
  + 第 3.5 节：“代码：物理内存分配器”
  + 第 8.1 至 8.3 节：“概述”、“缓冲区缓存层”和“代码：缓冲区缓存”
## 第六章：锁
+ 锁提供了互斥的功能，确保一次只有一个CPU可以持有一个特定的锁
+ 如果程序员为每个共享数据项关联一个锁，并且代码在使用某项时总是持有关联的锁
  + 那么该项每次只能由一个CPU使用
  + 在这种情况下，我们说锁保护了数据项
+ 虽然锁是一种简单易懂的并发控制机制，但其也带来了性能降低的缺点，因为锁将并发操作串行化了
### 竞争条件
<img src=".\picture\image.png">

+ 考虑两个进程在两个不同的CPU上调用wait，wait释放子进程的内存
  + 因此，在每个CPU上，内核都会调用kfree来释放子进程的内存页
  + 内核分配器维护了一个链表:
    + kalloc()从空闲页链表中pop一页内存
    + kfree()将一页push空闲链表中
  + 为了达到最好的性能，我们可能希望两个父进程的kfree能够并行执行，而不需要任何一个进程等待另一个进程，但是考虑到xv6的kfree实现，这是不正确的
+ 如图所示：
  + 链表在两个CPU共享的内存中，CPU使用加载和存储指令操作链表
  + (在现实中，处理器有缓存，但在概念上，多处理器系统的行为就像有一个单一的共享内存一样)
  + 如果没有并发请求，你可能会实现如下的链表push操作:
```c
struct element{
	int data;
	struct element *next;
};

struct element *list = 0;

void
push(int data)
{
	struct element *l;

	l = malloc(sizeof *l);
    l->data = data;
    l->next = list;
	list = l;
}
```
<img src=".\picture\image1.png">

+ 如果单独执行，这个实现是正确的
+ 但是，如果多个副本同时执行，代码就不正确
  + 如果两个CPU同时执行push，那么两个CPU可能都会执行图6.1所示的第15行
  + 然后其中一个才执行第16行，这就会产生一个不正确的结果
    + 如图6.2所示
  + 这样就会出现两个list元素，将next设为list的前值
  + 当对list的两次赋值发生在第16行时，第二次赋值将覆盖第一次赋值
+ 第16行的丢失更新是竞争条件(race condition)的一个例子
  + 竞争条件是指同时访问一个内存位置，并且至少有一次访问是写的情况
  + 竞争通常是一个错误的标志
    + 要么是丢失更新(如果访问是写)
    + 要么是读取一个不完全更新的数据结构
+ 避免竞争的通常方法是使用锁
  + 锁确保了相互排斥
  + 因此一次只能有一个CPU执行push的哪一行;这就使得上面的情况不可能发生
```c
struct element *list=0;
struct locklist lock;

void
push (int data)
{
	struct element *l;

	l = malloc(sizeof *l);
    l->data = data;
	acquire(&listlock);
    l->next = list;
	list = l;
    release(&listlock);
}
```
+ acquire和release之间的指令序列通常被称为临界区。这里的锁保护list
+ 当我们说锁保护数据时
  + 我们真正的意思是锁保护了一些适用于数据的不变量(invariant)的集合
  + 不变量是数据结构的属性
    + 这些属性在不同的操作中得到维护
  + 通常情况下，一个操作的正确行为取决于操作开始时的不变量是否为真
  + 操作可能会暂时违反不变量，但在结束前必须重新建立不变量
    + 例如，在链表中，不变性是：“list指向列表中的第一个元素，并且每个元素的下一个字段指向下一个元素”
    + push的实现暂时违反了这一不变性：
      + 在第17行，l指向下一个链表元素list，但list还没有指向l（在第18行重新建立）
+ 可以把锁看成是把并发的关键部分序列化，使它们一次只运行一个，从而保存不变性（假设关键部分孤立地正确）
  + 也可以认为由同一个锁保护的关键部分是相互原子的(atomic)
    + 因此每个关键部分只看到来自更早的关键部分的完整变化，而永远不会看到部分完成的更新
+ 如果多个进程同时想要同一个锁，就会发生冲突，或者说锁经历了争夺
  + 内核设计的一个主要挑战是避免锁的争夺
  + Xv6在这方面做得很少，但是复杂的内核会专门组织数据结构和算法来避免锁争用
    + 在列表的例子中，一个内核可能会维护每个CPU的空闲列表
    + 只有当CPU的列表是空的，并且它必须从另一个CPU偷取内存时，才会接触另一个CPU的空闲列表
+ 锁的位置对性能也很重要
  + 例如，在push中把acquisition移动到较早的位置也是正确的：
    + 将acquisition的调用移动到第13行之前是可以的
  + 然而，这可能会降低性能
    + 因为这样的话，对malloc的调用也会被序列化
    + 下面的“使用锁”一节提供了一些关于在哪里插入acquisition和release调用的指南
### 代码：锁
+ Xv6有两种类型的锁:
  + 自旋锁(spinlock)和睡眠锁(sleeplock)
--------------------------------------------
+ Xv6将自旋锁表示为一个结构体spinlock(kernel/spinlock.h:2)
  + 该结构中重要的字段是locked
    + 当锁可获得时，locked为零
    + 当锁被持有时，locked为非零
+ 从逻辑上讲，xv6获取锁的的代码类似于:
```c
void
acquire(struct spinlock *lk)//doesnotwork!
{
	for(;;){
		if(lk->locked == 0){
			lk->locked = 1;
			break;
		}
	}
}
```
+ 这种实现并不能保证多处理器上的相互排斥
  + 可能会出现这样的情况:
    + 两个CPU同时到达if语句，看到lk->locked为零，然后都通过设置lk->locked=1来抢夺锁
    + 此时，两个不同的CPU持有锁，这就违反了互斥属性
+ 由于锁被广泛使用，多核处理器通常提供了一些原子版的指令
  + 在RISC-V上，这条指令是amoswap a
    + amoswap读取内存地址a处的值，将寄存器r的内容写入该地址，并将其读取的值放入r中
    + 也就是说，它将寄存器的内容和内存地址进行了交换
    + 它原子地执行这个序列，使用特殊的硬件来防止任何其他CPU使用读和写之间的内存地址
+ Xv6的acquire使用了可移植的C库调用__sync_lock_test_and_set
  + 它本质上为amoswap指令
  + 返回值是lk->locked的旧(被交换出来的)内容
  + acquire函数循环交换，重试(旋转)直到获取了锁
  + 每一次迭代都会将1交换到lk->locked中，并检查之前的值
    + 如果之前的值为0，那么我们已经获得了锁，并且交换将lk->locked设置为1
    + 如果之前的值是1，那么其他CPU持有该锁，而我们原子地将1换成lk->locked并没有改变它的值
  + 一旦锁被获取，acquire就会记录获取该锁的CPU，这方便调试
    + lk->cpu字段受到锁的保护，只有在持有锁的时候才能改变
+ 函数release与acquire相反:
  + 它清除lk->cpu字段，然后释放锁
    + 从概念上讲，释放只需要给lk->locked赋值为0
  + C标准允许编译器用多条存储指令来实现赋值，所以C赋值对于并发代码来说可能是非原子性的
  + 相反，release使用C库函数__sync_lock_release执行原子赋值
    + 这个函数也是使用了RISC-V的amoswap指令
### 代码：使用锁
+ 使用锁的一个难点是决定使用多少个锁，以及每个锁应该保护哪些数据和不变量
+ 有几个基本原则：
  + 任何时候，当一个CPU可以在另一个CPU读或写变量的同时写入变量时，都应该使用锁来防止这两个操作重叠
  + 记住锁保护不变量：
    + 如果一个不变量涉及多个内存位置，通常需要用一个锁保护所有的位置，以确保不变式得到维护
+ 不需要的锁的情况：
  + 如果并行性不重要，那么可以安排只有一个进程，而不用担心锁的问题
    + 作为粗粒度锁的一个例子，xv6的kalloc.c分配器有一个单一的空闲列表，由一个单一的锁构成
      + 如果不同CPU上的多个进程试图同时分配页面，那么每个进程都必须通过在acquire中旋转来等待轮到自己
    + 旋转会降低性能，因为这不是有用的工作
    + 如果争夺锁浪费了相当一部分CPU时间，也许可以通过改变分配器的设计来提高性能
      + 使其拥有多个空闲列表，每个列表都有自己的锁，从而实现真正的并行分配
      + （该分配器在lockinglab中会被重写）
    + 作为细粒度锁的一个例子
      + xv6为每个文件都有一个单独的锁，这样操作不同文件的进程往往可以不用等待对方的锁就可以进行
      + 如果想让进程模拟写入同一文件的不同区域，文件锁方案可以做得更细
      + 最终，锁的粒度决定需要由性能测量以及复杂性考虑来驱动
### 死锁和锁的顺序
+ 如果一个穿过内核的代码路径必须同时持有多个锁，那么所有的代码路径以相同的顺序获取这些锁是很重要的
  + 如果他们不这样做，就会有死锁的风险
+ 假设线程T1执行代码path1并获取锁A，线程T2执行代码path2并获取锁B
  + 接下来T1会尝试获取锁B，T2会尝试获取锁A
  + 这两次获取都会无限期地阻塞，因为在这两种情况下，另一个线程都持有所需的锁，并且不会释放它，直到它的获取返回
+ 为了避免这样的死锁，所有的代码路径必须以相同的顺序获取锁
  + 对全局锁获取顺序的需求意味着锁实际上是每个函数规范的一部分:
    + 调用者调用函数的方式必须使锁按照约定的顺序被获取
+ 由于sleep的工作方式，xv6有许多长度为2的锁序链，涉及到进程锁(structproc中的锁)
  + 例如，consoleintr(kernel/console.c:138)是处理格式化字符的中断流程
  + 当一个新数据到达时，任何正在等待控制台（终端，即cmd）输入的进程都应该被唤醒
  + 为此，consoleintr在调用wakeup时持有cons.lock，以获取进程锁来唤醒它
  + 因此，全局避免死锁的锁顺序包括了cons.lock必须在任何进程锁之前获取的规则
  + 文件系统代码包含xv6最长的锁链
    + 例如，创建一个文件需要同时持有
      + 目录的锁
      + 新文件的inode的锁
      + 磁盘块缓冲区的锁
      + 磁盘驱动器的vdisk_lock
      + 调用进程的p->lock
    + 为了避免死锁，文件系统代码总是按照上一句提到的顺序获取锁
+ 有时锁的顺序与逻辑程序结构相冲突
  + 例如，也许代码模块M1调用模块M2，但锁的顺序要求M2中的锁在M1中的锁之前被获取
  + 有时锁的身份并不是事先知道的，也许是因为必须持有一个锁才能发现接下来要获取的锁的身份
    + 这种情况出现在文件系统中，因为它在路径名中查找连续的组件
    + 也出现在wait和exit的代码中，因为它们搜索进程表寻找子进程
  + 最后，死锁的危险往往制约着人们对锁方案的细化程度
    + 因为更多的锁往往意味着更多的死锁机会
    + 避免死锁是内核实现的重要需求
### 锁与中断处理
+ 一些xv6自旋锁保护的数据会被线程和中断处理程序两者使用
  + 例如，clockintr定时器中断处理程序可能会在内核线程读取sys_sleep中的ticks的同时，递增ticks(kernel/trap.c:163)
  + 锁tickslock将保护两次临界区
+ 自旋锁和中断的相互作用带来了一个潜在的危险
  + 假设sys_sleep持有tickslock，而它的CPU接收到一个时钟中断
  + clockintr会尝试获取tickslock，看到它被持有，并等待它被释放
  + 在这种情况下，tickslock永远不会被释放：
    + 只有sys_sleep可以释放它，但sys_sleep不会继续运行，直到clockintr返回
    + 所以CPU会死锁，任何需要其他锁的代码也会冻结
+ 为了避免这种情况，如果一个中断处理程序使用了自旋锁
  + CPU决不能在启用中断的情况下持有该锁
  + Xv6则采用了更加保守的策略：
    + 当一个CPU获取任何锁时，xv6总是禁用该CPU上的中断
    + 中断仍然可能发生在其他CPU上，所以一个中断程序获取锁会等待一个线程释放自旋锁，但它们不在同一个CPU上
+ xv6在CPU没有持有自旋锁时重新启用中断
  + 它必须做一点记录来应对嵌套的临界区
  + acquire调用push_off和release调用pop_off来跟踪当前CPU上锁的嵌套级别
  + 当该计数达到零时，pop_off会恢复最外层临界区开始时的中断启用状态
  + intr_off和intr_on函数分别执行RISC-V指令来禁用和启用中断
+ 在设置lk->locked之前，严格调用push_off是很重要的
  + 如果两者反过来，那么在启用中断的情况下，锁会有一个窗口(未锁到的位置)，在未禁止中断时持有锁
  + 在这种情况下，一个时机恰到好处的时钟中断会使系统死锁
  + 同样，释放锁后才调用pop_off也很重要
### 指令与存储的顺序
+ 许多编译器和CPU为了获得更高的性能，会不按顺序执行代码
+ 如果一条指令需要很多周期才能完成，CPU可能会提前发出该指令，以便与其他指令重叠，避免CPU停顿
  + 例如，CPU可能会注意到在一个串行序列中，指令A和B互不依赖
  + CPU可能先启动指令B，这是因为它的输入在A的输入之前已经准备好了，或者是为了使A和B的执行重叠
  + 编译器可以执行类似的重新排序，在一条语句的指令之前发出另一条语句的指令，由于它们原来的顺序
+ 编译器和CPU在对指令重新排序时遵循相应规则，以确保它们不会改变正确编写的串行代码的结果
  + 然而，这些规则确实允许重排，从而改变并发代码的结果，并且很容易导致多处理器上的不正确行为
  + CPU的指令排序规则规则称为内存模型(memory model)
  + 例如，在这段push的代码中，如果编译器或CPU将第4行对应的存储移到第6行释放后的某个点，那将是一场灾难
```c
l = malloc(sizeof *l);
l->data = data;
acquire(&listlock);
l->next = list;
list = l;
release(&listlock);
```
+ 如果发生这样的重排，就会有一个指令执行的窗口
  + 在这个窗口中，另一个CPU可以获取锁并观察更新的链表，但看到的是一个未初始化的list->next
+ 为了告诉硬件和编译器不要执行这样的re-ordering，xv6在acquire和release中都使用了__sync_synchronize()
  + __sync_synchronize()是一个内存屏障(memory barrier):
    + 它告诉编译器和CPU不要在越过屏障重新排列任何的内存读写操作
    + acquire和release中的屏障几乎在所有重要的情况下都会强制锁定顺序，因为xv6在访问共享数据的周围使用锁
### 睡眠锁
+ 有时xv6需要长时间保持一个锁
  + 例如，文件系统在磁盘上读写文件内容时，会保持一个文件的锁定，这些磁盘操作可能需要几十毫秒
  + 如果另一个进程想获取一个自旋锁，那么保持那么长的时间会导致浪费
    + 因为第二个进程在等待锁的同时会浪费CPU很长时间
  + 自旋锁的另一个缺点是
    + 一个进程在保留自旋锁的同时不能释放CPU并将自身转变为就绪态
    + 我们希望做到这一点，以便在拥有自旋锁的进程等待磁盘时，其他进程可以使用CPU
    + 在持有自旋锁时释放CPU是非法的
      + 因为如果第二个线程再试图获取自旋锁，可能会导致死锁
      + 由于acquire并不能释放CPU，第二个进程的等待可能会阻止第一个进程运行和释放锁
      + 在持有锁的同时释放CPU也会违反在持有自旋锁时中断必须关闭的要求
+ 因此，我们希望有一种锁:**在等待获取的同时让CPU可以进行别的工作，并在锁被持有时允许释放CPU，同时开放中断**
+ Xv6以睡眠锁(sleeplock)的形式提供了这样的锁
  + 在高层次上，睡眠锁有一个由spinlock保护的锁定字段
  + acquiresleep调用sleep原子性地让渡CPU并释放spinlock
  + 结果就是，在acquireleep等待的时候，其他线程可以执行
    + 因为睡眠锁使中断处于启用状态，所以它们不能用于中断处理程序中
    + 由于acquiresleep可能会释放CPU，所以睡眠锁不能在自旋锁的核心代码中使用（尽管自旋锁可以在睡眠锁的核心代码中使用）
    + 自旋锁最适合于短的关键部分，因为等待它们会浪费CPU时间
    + 睡眠锁对长的操作很有效
## 第 3.5 节：“代码：物理内存分配器”
+ 分配器在 kalloc.c（kernel/kalloc.c:1）中
  + 分配器的数据结构是一个可供分配的物理内存页的空闲链表，每个空闲页的链表元素是一个结构体 struct run（kernel/kalloc.c:17）
  + 分配器从哪里获得内存来存放这个结构体呢？
    + 它把每个空闲页的 run 结构体存储在空闲页自身里面，因为那里没有其他东西存储
    + 空闲链表由一个自旋锁保护
    + 链表和锁被包裹在一个结构体中，以明确锁保护的是结构体中的字段
+ main 调用 kinit 来初始化分配器
  + kinit 初始化空闲页链表，以保存内核地址结束到 PHYSTOP 之间的每一页
  + xv6 应该通过解析硬件提供的配置信息来确定有多少物理内存可用
    + 但是它没有这么做，而是假设机器有 128M 字节的 RAM
    + kinit 通过调用freerange 来添加内存到空闲页链表，freerange 则对每一页都调用 kfree
    + PTE 只能指向按4096 字节对齐的物理地址（4096 的倍数）
      + 因此 freerange 使用 PGROUNDUP 来确保它只添加对齐的物理地址到空闲链表中
  + 分配器开始时没有内存
    + 这些对 kfree 的调用给了它一些内存管理
+ 分配器有时把地址当作整数来处理
  + 以便对其进行运算（如 freerange 遍历所有页）
+ 有时把地址当作指针来读写内存（如操作存储在每页中的 run 结构体）
+ 这种对地址的双重使用是分配器代码中充满 C 类型转换的主要原因
  + 另一个原因是，释放和分配本质上改变了内存的类型
+ kfree将被释放的内存中的每个字节设置为1
  + 这将使得释放内存后使用内存的代码（使用悬空引用）将会读取垃圾而不是旧的有效内容
  + 希望这将导致这类代码更快地崩溃
+ 然后 kfree 将页面预存入释放列表：
  + 它将 pa（物理地址）转为指向结构体 run 的指针
  + 在 r->next 中记录空闲链表之前的节点
  + 并将释放列表设为 r
+ kalloc移除并返回空闲链表中的第一个元素
## 第八章：文件系统
+ 文件系统的目的是组织和存储数据
  + 文件系统通常支持用户和应用程序之间的数据共享，以及支持持久性，以便数据在重启后仍然可用
+ xv6文件系统提供了类Unix的文件、目录和路径名，并将其数据存储在virtio磁盘上以实现持久化
  + 该文件系统解决了几个挑战：
    + 文件系统需要磁盘上的数据结构来表示命名目录和文件的树，记录保存每个文件内容的块的身份，并记录磁盘上哪些区域是空闲的
    + 文件系统必须支持崩溃恢复
      + 也就是说，如果发生崩溃（如电源故障），文件系统必须在重新启动后仍能正常工作
      + 风险在于，崩溃可能会中断更新序列，并在磁盘上留下不一致的数据结构
      + （例如，一个块既在文件中使用，又被标记为空闲）
      + 不同的进程可能并发在文件系统上运行
        + 所以文件系统代码必须协调维护每一个临界区
      + 访问磁盘的速度比访问内存的速度要慢几个数量级，所以文件系统必须在内存维护一个缓冲区，用于缓存常用块
### 概述
<img src=".\picture\image2.png">

+ xv6文件系统的实现分为七层，如图8.1所示
+ disk层在virtio磁盘上读写块
+ Buffer cache缓存磁盘块，并同步访问它们
  + 确保一个块只能同时被内核中的一个进程访问
+ 日志层允许上层通过事务更新多个磁盘块，并确保在崩溃时，磁盘块是原子更新的（即全部更新或不更新）
+ inode层将一个文件都表示为一个inode
  + 每个文件包含一个唯一的i-number和一些存放文件数据的块
+ 目录层将实现了一种特殊的inode
  + 被称为目录
  + 其包含一个目录项序列，每个目录项由文件名称和i-number组成
  + 路径名层提供了层次化的路径名，如/usr/rtm/xv6/fs.c，可以用递归查找解析他们
  + 文件描述符层用文件系统接口抽象了许多Unix资源（如管道、设备、文件等），使程序员的生产力得到大大的提高
<img src=".\picture\image3.png">

+ 文件系统必须安排好磁盘存储inode和内容块的位置
+ 为此，xv6将磁盘分为几个部分，如图8.2所示
+ 文件系统不使用块0（它存放boot sector）
+ 第1块称为superblock
  + 它包含了文件系统的元数据（以块为单位的文件系统大小、数据块的数量、inode的数量和日志中的块数）
+ 从块2开始存放着日志
  + 日志之后是inodes，每个块会包含多个inode
  + 在这些块之后是位图块(bitmap)，记录哪些数据块在使用
  + 其余的块是数据块，每个数据块
    + 要么在bitmap块中标记为空闲
    + 要么持有文件或目录的内容
  + 超级块由一个单独的程序mkfs写入，它建立了一个初始文件系统
### 缓冲区缓存层
+ buffer缓存有两项工作
  + 同步访问磁盘块，以确保磁盘块在内存中只有一个buffer缓存，并且一次只有一个内核线程能使用该buffer缓存
  + 缓存使用较多的块，这样它们就不需要从慢速磁盘中重新读取
+ buffer缓存的主要接口包括bread和bwrite
  + bread返回一个在内存中可以读取和修改的块副本buf
  + bwrite将修改后的buffer写到磁盘上相应的块
    + 内核线程在使用完一个buffer后，必须通过调用brelse释放它
    + buffer缓存为每个buffer的都设有sleep-lock，以确保每次只有一个线程使用buffer（从而使用相应的磁盘块）
    + bread 返回的buffer会被锁定，而brelse释放锁
+ buffer缓存有固定数量的buffer来存放磁盘块
  + 这意味着如果文件系统需要一个尚未被缓存的块
  + buffer缓存必须回收一个当前存放其他块的buffer
+ buffer缓存为新块寻找最近使用最少的buffer（lru机制）
  + 因为最近使用最少的buffer是最不可能被再次使用的buffer
### 代码：缓冲区缓存
+ buffer缓存是一个由buffer组成的双端链表
  + 由函数binit用静态数组buf初始化这个链表， binit在启动时由main调用
  + 访问buffer缓存是通过链表，而不是buf数组
+ buffer有两个与之相关的状态字段
  + 字段valid表示是否包含该块的副本（是否从磁盘读取了数据）
  + 字段disk表示缓冲区的内容已经被修改需要被重新写入磁盘
+ bget扫描buffer链表
  + 寻找给定设备号和扇区号来查找缓冲区
  + 如果存在，bget就会获取该buffer的sleep-lock
  + 然后bget返回被锁定的buffer
+ 如果给定的扇区没有缓存的buffer
  + bget必须生成一个
  + 可能会使用一个存放不同扇区的buffer，它再次扫描buffer链表，寻找没有被使用的buffer(b->refcnt = 0)
+ 任何这样的buffer都可以使用
  + bget修改buffer元数据，记录新的设备号和扇区号，并获得其sleep-lock
  + 请注意，b->valid = 0可以确保bread从磁盘读取块数据，而不是错误地使用buffer之前的内容
+ 请注意，**每个磁盘扇区最多只能有一个buffer，以确保写操作对读取者可见**
  + 也因为文件系统需要使用buffer上的锁来进行同步
  + Bget通过从第一次循环检查块是否被缓存
  + 第二次循环来生成一个相应的buffer（通过设置dev、blockno和refcnt）
  + 在进行这两步操作时，需要一直持有bache.lock
  + 持有bache.lock会保证上面两个循环在整体上是原子的
+ bget在bcache.lock保护的临界区之外获取buffer的sleep-lock是安全的
  + 因为非零的b->refcnt可以防止缓冲区被重新用于不同的磁盘块
  + sleep-lock保护的是块的缓冲内容的读写，而bcache.lock保护被缓存块的信息
+ 如果所有buffer都在使用，那么太多的进程同时在执行文件相关的系统调用，bget就会panic
  + 一个更好的处理方式可能是睡眠，直到有buffer空闲，尽管这时有可能出现死锁
+ 一旦bread读取了磁盘内容（如果需要的话）并将缓冲区返回给它的调用者
  + 调用者就独占该buffer，可以读取或写入数据
  + 如果调用者修改了buffer，它必须在释放buffer之前调用bwrite将修改后的数据写入磁盘
    + bwrite调用virtio_disk_rw与磁盘硬件交互
+ 当调用者处理完一个buffer后，必须调用brelse来释放它
  + brelse释放sleep-lock，并将该buffer移动到链表的头部
  + 移动buffer会使链表按照buffer最近使用的时间（最近释放）排序
    + 链表中的第一个buffer是最近使用的
    + 最后一个是最早使用的
  + bget中的两个循环利用了这一点
    + 在最坏的情况下，获取已缓存buffer的扫描必须处理整个链表
    + 由于数据局部性，先检查最近使用的缓冲区（从bcache.head开始，通过next指针）将减少扫描时间
    + 扫描选取可使用buffer的方法是通过从后向前扫描（通过prev指针）选取最近使用最少的缓冲区