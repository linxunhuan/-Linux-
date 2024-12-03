# 多处理器和锁定
## 为什么要使用锁？
+ 当并行的访问数据结构时
  + 例如一个核在读取数据，另一个核在写入数据
  + 我们需要使用锁来协调对于共享数据的更新，以确保数据的一致性
+ 所以，我们需要锁来控制并确保共享的数据是正确的。
+ 但是实际的情况有些令人失望：
  + 我们想要通过并行来获得高性能
  + 我们想要并行的在不同的CPU核上执行系统调用
  + 但是如果这些系统调用使用了共享的数据，我们又需要使用锁
  + 而锁又会使得这些系统调用串行执行，所以最后锁反过来又限制了性能
----------------------------------------------
+ 先来看看什么是race condition
+ 在XV6中创建一个race condition，然后看看它的表象是什么
+ kalloc.c文件中的kfree函数会将释放的page保存于freelist中
```c
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}
```
+ freelist是XV6中的一个非常简单的数据结构
  + 它会将所有的可用的内存page保存于一个列表中
  + 这样当kalloc函数需要一个内存page时，它可以从freelist中获取
+ 从函数中可以看出，这里有一个锁kmem.lock，在加锁的区间内，代码更新了freelist
+ 现在我们将锁的acquire和release注释上
  + 这样原来在上锁区间内的代码就不再受锁保护
  + 并且不再是原子执行的
+ 之后运行make qemu重新编译XV6，
+ race condition可以有不同的表现形式，并且它可能发生，也可能不发生
  + 但是在这里的usertests中，很明显发生了什么
  + (这段懒得放图了，但是只要做了实验就懂啥意思)
## 锁如何避免race condition
+ 假设两个CPU核在相同的时间调用kfree
+ kfree函数接收一个物理地址pa作为参数
  + freelist是个单链表
  + kfree中将pa作为单链表的新的head节点，并更新freelist指向pa
    + （注，也就是将空闲的内存page加在单链表的头部）
  + 当两个CPU都调用kfree时
    + CPU0想要释放一个page
    + CPU1也想要释放一个page
    + 现在这两个page都需要加到freelist中
+ kfree中首先将对应内存page的变量r指向了当前的freelist（也就是单链表当前的head节点）
  + 假设CPU0先运行
    + 那么CPU0会将它的变量r的next指向当前的freelist
  + 如果CPU1在同一时间运行
    + 它可能在CPU0运行第二条指令（kmem.freelist = r）之前运行代码
    + 所以它也会完成相同的事情，它会将自己的变量r的next指向当前的freelist
  + 现在两个物理page对应的变量r都指向了同一个freelist（也就是原来单链表的head节点）
+ 接下来，剩下的代码也会并行的执行（kmem.freelist = r）
  + 这行代码会更新freelist为r
  + 因为这里只有一个内存，所以总是有一个CPU会先执行，另一个后执行
    + 假设CPU0先执行，那么freelist会等于CPU0的变量r
    + 之后CPU1再执行，它又会将freelist更新为CPU1的变量r
  + 这样的结果是，我们丢失了CPU0对应的page
    + CPU0想要释放的内存page最终没有出现在freelist数据中
+ 这是一种具体的坏的结果，当然可能会有更多坏的结果，因为可能会有更多的CPU
  + 例如第三个CPU可能会短暂的发现freelist等于CPU0对应的变量r，并且使用这个page，但是之后很快freelist又被CPU1更新了
  + 所以，拥有越多的CPU，我们就可能看到比丢失page更奇怪的现象
--------------------------------------------------
+ 在代码中，用来解决这里的问题的最常见方法就是使用锁
  + 锁就是一个对象，就像其他在内核中的对象一样
  + 有一个结构体叫做lock，它包含了一些字段，这些字段中维护了锁的状态
  + 锁有非常直观的API：
    + acquire，接收指向lock的指针作为参数
      + acquire确保了在任何时间，只会有一个进程能够成功的获取锁
    + release，也接收指向lock的指针作为参数
      + 在同一时间尝试获取锁的其他进程需要等待，直到持有锁的进程对锁调用release
+ 锁的acquire和release之间的代码，通常被称为critical section
  + 这里以原子的方式执行共享数据的更新
  + 所以基本上来说，如果在acquire和release之间有多条指令
    + 它们要么会一起执行，要么一条也不会执行
    + 所以永远也不可能看到位于critical section中的代码，如同在race condition中一样在多个CPU上交织的执行
    + 所以这样就能避免race condition
+ 为什么会有这么多锁呢？
  + 因为锁序列化了代码的执行
    + 如果两个处理器想要进入到同一个critical section中，只会有一个能成功进入，另一个处理器会在第一个处理器从critical section中退出之后再进入
    + 所以这里完全没有并行执行
  + 如果内核中只有一把大锁，我们暂时将之称为big kernel lock
    + 基本上所有的系统调用都会被这把大锁保护而被序列化
    + 系统调用会按照这样的流程处理：
      + 一个系统调用获取到了big kernel lock，完成自己的操作，之后释放这个big kernel lock
      + 再返回到用户空间，之后下一个系统调用才能执行
    + 这样的话，如果我们有一个应用程序并行的调用多个系统调用
      + 这些系统调用会串行的执行，因为我们只有一把锁
    + 所以通常来说，例如XV6的操作系统会有多把锁
      + 这样就能获得某种程度的并发执行
      + 如果两个系统调用使用了两把不同的锁，那么它们就能完全的并行运行
## 什么时候使用锁？
**如果两个进程访问了一个共享的数据结构，并且其中一个进程会更新共享的数据结构，那么就需要对于这个共享的数据结构加锁**
+ 矛盾的是，有时候这个规则太过严格，而有时候这个规则又太过宽松了
----------------------------------------
+ 太过宽松：
+ 除了共享的数据，在一些其他场合也需要锁
  + 例如对于printf，如果我们将一个字符串传递给它，XV6会尝试原子性的将整个字符串输出，而不是与其他进程的printf交织输出
  + 尽管这里没有共享的数据结构，但在这里锁仍然很有用处
    + 因为我们想要printf的输出也是序列化的
------------------------------------------
+ 太过严格：
+ 假设我们有一个对于rename的调用，这个调用会将文件从一个目录移到另一个目录，我们现在将文件d1/x移到文件d2/y
+ 如果我们按照前面说的，对数据结构自动加锁
+ 现在我们有两个目录对象，一个是d1，另一个是d2
  + 那么我们会先对d1加锁，删除x
  + 之后再释放对于d1的锁
  + 之后我们会对d2加锁，增加y
  + 之后再释放d2的锁
+ 在我们完成了第一步，也就是删除了d1下的x文件，但是还没有执行第二步，也就是创建d2下的y文件时
  + 其他的进程会看到文件完全不存在
  + 这明显是个错误的结果，因为文件还存在只是被重命名了，文件在任何一个时间点都是应该存在的
  + 但是如果我们按照上面的方式实现锁的话，那么在某个时间点，文件看起来就是不存在的
+ 所以这里正确的解决方法是，我们在重命名的一开始就对d1和d2加锁，之后删除x再添加y，最后再释放对于d1和d2的锁
+ 在这个例子中，锁应该与操作而不是数据关联，所以自动加锁在某些场景下会出问题
## 锁的特性和死锁
+ 通常锁有三种作用
  + 锁可以避免丢失更新
    + 如果你回想我们之前在kalloc.c中的例子，丢失更新是指我们丢失了对于某个内存page在kfree函数中的更新
    + 如果没有锁，在出现race condition的时候，内存page不会被加到freelist中
    + 但是加上锁之后，我们就不会丢失这里的更新
  + 锁可以打包多个操作，使它们具有原子性
    + 我们之前介绍了加锁解锁之间的区域是critical section
    + 在critical section的所有操作会都会作为一个原子操作执行
  + 锁可以维护共享数据结构的不变性
    + 共享数据结构如果不被任何进程修改的话是会保持不变的
    + 如果某个进程acquire了锁并且做了一些更新操作，共享数据的不变性暂时会被破坏
    + 但是在release锁之后，数据的不变性又恢复了
    + 之前在kfree函数中的freelist数据，所有的free page都在一个单链表上
      + 但是在kfree函数中，这个单链表的head节点会更新
-------------------------------
+ 锁可能带来的一些缺点
+ 不恰当的使用锁，可能会带来一些锁特有的问题。最明显的一个例子就是死锁（Deadlock）
+ 一个死锁的最简单的场景：
  + 首先acquire一个锁，然后进入到critical section
  + 在critical section中，再acquire同一个锁
  + 第二个acquire必须要等到第一个acquire状态被release了才能继续执行
  + 但是不继续执行的话又走不到第一个release，所以程序就一直卡在这了
  + 这就是一个死锁
+ XV6会探测这样的死锁，如果XV6看到了同一个进程多次acquire同一个锁，就会触发一个panic
+ 当有多个锁的时候，场景会更加有趣
  + 假设现在我们有两个CPU，一个是CPU1，另一个是CPU2
  + CPU1执行rename将文件d1/x移到d2/y，CPU2执行rename将文件d2/a移到d1/b
  + 这里CPU1将文件从d1移到d2，CPU2正好相反将文件从d2移到d1
  + 我们假设我们按照参数的顺序来acquire锁
    + 那么CPU1会先获取d1的锁，如果程序是真正的并行运行，CPU2同时也会获取d2的锁
    + 之后CPU1需要获取d2的锁，这里不能成功，因为CPU2现在持有锁，所以CPU1会停在这个位置等待d2的锁释放
    + 而另一个CPU2，接下来会获取d1的锁，它也不能成功，因为CPU1现在持有锁
  + 这也是死锁的一个例子，有时候这种场景也被称为deadly embrace。这里的死锁就没那么容易探测了
+ 这里的解决方案是，如果你有多个锁，你需要对锁进行排序，所有的操作都必须以相同的顺序获取锁
  + 例如在这里的例子中我们让d1一直在d2之前
  + 这样我们在rename的时候，总是先获取排序靠前的目录的锁，再获取排序靠后的目录的锁
  + 如果对于所有的锁有了一个全局的排序，这里的死锁就不会出现了
+ 不过在设计一个操作系统的时候，定义一个全局的锁的顺序会有些问题
  + 如果一个模块m1中方法g调用了另一个模块m2中的方法f
    + 那么m1中的方法g需要知道m2的方法f使用了哪些锁
      + 因为如果m2使用了一些锁，那么m1的方法g必须集合f和g中的锁，并形成一个全局的锁的排序
    + 这意味着在m2中的锁必须对m1可见，这样m1才能以恰当的方法调用m2
  + 但是这样又违背了代码抽象的原则
    + 在完美的情况下，代码抽象要求m1完全不知道m2是如何实现的
  + 但是不幸的是，具体实现中，m2内部的锁需要泄露给m1，这样m1才能完成全局锁排序
+ 所以当你设计一些更大的系统时，锁使得代码的模块化更加的复杂了
## 锁与性能
+ 基本上来说，如果你想获得更高的性能，你需要拆分数据结构和锁
+ 如果你只有一个big kernel lock，那么操作系统只能被一个CPU运行
+ 如果你想要性能随着CPU的数量增加而增加，你需要将数据结构和锁进行拆分
+ 那怎么拆分呢？通常不会很简单，有的时候还有些困难
  + 比如说，你是否应该为每个目录关联不同的锁？
  + 你是否应该为每个inode关联不同的锁？
  + 你是否应该为每个进程关联不同的锁？
  + 或者是否有更好的方式来拆分数据结构呢？
+ 如果你重新设计了加锁的规则，你需要确保不破坏内核一直尝试维护的数据不变性
+ 如果你拆分了锁，你可能需要重写代码
+ 如果你为了获得更好的性能
  + 重构了部分内核或者程序，将数据结构进行拆分并引入了更多的锁
  + 这涉及到很多工作，你需要确保你能够继续维持数据的不变性，你需要重写代码
+ 所以这里就有矛盾点了
  + 我们想要获得更好的性能，那么我们需要有更多的锁，但是这又引入了大量的工作
+ 通常来说，开发的流程是：
  + 先以coarse-grained lock（注，也就是大锁）开始
  + 再对程序进行测试，来看一下程序是否能使用多核
  + 如果可以的话，那么工作就结束了，你对于锁的设计足够好了；如果不可以的话，那意味着锁存在竞争，多个进程会尝试获取同一个锁，因此它们将会序列化的执行，性能也上不去，之后你就需要重构程序
+ 在这个流程中，测试的过程比较重要
  + 有可能模块使用了coarse-grained lock，但是它并没有经常被并行的调用，那么其实就没有必要重构程序
  + 因为重构程序设计到大量的工作，并且也会使得代码变得复杂
  + 所以如果不是必要的话，还是不要进行重构
## XV6中UART模块对于锁的使用
+ 首先查看一下uart.c，在上节课介绍中断的时候我们提到了这里的锁，现在我们具体的来看一下
+ 因为现在我们对锁更加的了解了，接下来将展示一些更有趣的细节
+ 从代码上看UART只有一个锁
```c
// the transmit output buffer
struct spinlock uart tx lock;
#define UART TX BUF SIZE 32
char uart tx buf[UART TX BUF SIZE];
int uart tx w; // write next to uart tx buf[uart tx w++]
int uart txr;// read next from uart tx buf[uar tx r++]
```
+ 所以你可以认为对于UART模块来说，现在是一个coarse-grained lock的设计
+ 这个锁保护了UART的的传输缓存；写指针；读指针
+ 当我们传输数据时，写指针会指向传输缓存的下一个空闲槽位，而读指针指向的是下一个需要被传输的槽位
+ 这是我们对于并行运算的一个标准设计，它叫做消费者-生产者模式
+ 所以现在有了一个缓存，一个写指针和一个读指针
  + 读指针的内容需要被显示，写指针接收来自例如printf的数据
+ 我们前面已经了解到了锁有多个角色
+ 第一个是保护数据结构的特性不变，数据结构有一些不变的特性
  + 例如读指针需要追赶写指针
  + 从读指针到写指针之间的数据是需要被发送到显示端
  + 从写指针到读指针之间的是空闲槽位，锁帮助我们维护了这些特性不变
-----------------------------------------------------
+ 接下来看一下uart.c中的uartputc函数
```c
// add a character to the output buffer and tell the
// UART to start sending if it isn't already.
// blocks if the output buffer is full.
// because it may block, it can't be called
// from interrupts; it's only suitable for use
// by write().
void
uartputc(int c)
{
  acquire(&uart_tx_lock);

  if(panicked){
    for(;;)
      ;
  }

  while(1){
    if(((uart_tx_w + 1) % UART_TX_BUF_SIZE) == uart_tx_r){
      // buffer is full.
      // wait for uartstart() to open up space in the buffer.
      sleep(&uart_tx_r, &uart_tx_lock);
    } else {
      uart_tx_buf[uart_tx_w] = c;
      uart_tx_w = (uart_tx_w + 1) % UART_TX_BUF_SIZE;
      uartstart();
      release(&uart_tx_lock);
      return;
    }
  }
}
```
+ 如果uart_tx_w不等于uart_tx_r，那么缓存不为空，说明需要处理缓存中的一些字符
+ 锁确保了我们可以在下一个字符写入到缓存之前，处理完缓存中的字符，这样缓存中的数据就不会被覆盖
+ 最后，锁确保了一个时间只有一个CPU上的进程可以写入UART的寄存器，THR
  + 所以这里锁确保了硬件寄存器只有一个写入者
+ 当UART硬件完成传输，会产生一个中断
+ 在前面的代码中我们知道了uartstart的调用者会获得锁以确保不会有多个进程同时向THR寄存器写数据
  + 但是UART中断本身也可能与调用printf的进程并行执行
  + 如果一个进程调用了printf，它运行在CPU0上
  + CPU1处理了UART中断，那么CPU1也会调用uartstart
+ 因为我们想要确保对于THR寄存器只有一个写入者，同时也确保传输缓存的特性不变（注，这里指的是在uartstart中对于uart_tx_r指针的更新），我们需要在中断处理函数中也获取锁
```c
// handle a uart interrupt, raised because input has
// arrived, or the uart is ready for more output, or
// both. called from trap.c.
void
uartintr(void)
{
  // read and process incoming characters.
  while(1){
    int c = uartgetc();
    if(c == -1)
      break;
    consoleintr(c);
  }

  // send buffered characters.
  acquire(&uart_tx_lock);
  uartstart();
  release(&uart_tx_lock);
}
```
+ 所以，在XV6中，驱动的bottom部分（注，也就是中断处理程序）和驱动的up部分（注，uartputc函数）可以完全的并行运行，所以中断处理程序也需要获取锁
##  自旋锁（Spin lock）的实现
+ 接下来我们看一下如何实现自旋锁。锁的特性就是只有一个进程可以获取锁，在任何时间点都不能有超过一个锁的持有者。我们接下来看一下锁是如何确保这里的特性。

我们先来看一个有问题的锁的实现，这样我们才能更好的理解这里的挑战是什么。实现锁的主要难点在于锁的acquire接口，在acquire里面有一个死循环，循环中判断锁对象的locked字段是否为0，如果为0那表明当前锁没有持有者，当前对于acquire的调用可以获取锁。之后我们通过设置锁对象的locked字段为1来获取锁。最后返回。



如果锁的locked字段不为0，那么当前对于acquire的调用就不能获取锁，程序会一直spin。也就是说，程序在循环中不停的重复执行，直到锁的持有者调用了release并将锁对象的locked设置为0。

在这个实现里面会有什么样的问题





































