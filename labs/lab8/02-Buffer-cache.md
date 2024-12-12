# 缓冲区缓存
## 官方题目
+ 如果多个进程密集使用文件系统，它们可能会争用bcache.lock
  + 该锁用于保护 kernel/bio.c 中的磁盘块缓存
+ bcachetest 创建了几个进程，这些进程反复读取不同的文件以生成对bcache.lock 的争用
+ 其输出如下所示（在完成本实验之前）：
```s
$ bcachetest
start test0
test0 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 0 #acquire() 33035
lock: bcache: #fetch-and-add 16142 #acquire() 65978
--- top 5 contended locks:
lock: virtio_disk: #fetch-and-add 162870 #acquire() 1188
lock: proc: #fetch-and-add 51936 #acquire() 73732
lock: bcache: #fetch-and-add 16142 #acquire() 65978
lock: uart: #fetch-and-add 7505 #acquire() 117
lock: proc: #fetch-and-add 6937 #acquire() 73420
tot= 16142
test0: FAIL
start test1
test1 OK
```
+ 您可能会看到不同的输出，但 bcache 锁的获取循环迭代次数会很高
+ 如果您查看 kernel/bio.c 中的代码
  + 您会看到 bcache.lock 保护缓存块缓冲区列表
  + 每个块缓冲区中的引用计数 (b->refcnt) 
  + 缓存块的标识 (b->dev 和 b->blockno)
+ 修改块缓存，使得运行bcachetest时，bcache 中所有锁的获取循环迭代次数接近于零
+ 理想情况下，块缓存中涉及的所有锁的计数总和应为零，但如果总和小于 500 也是可以的
+ 修改bget 和brelse，使得 bcache 中不同块的并发查找和释放不太可能在锁上发生冲突（例如，不必全部等待 bcache.lock）
+ 您必须保持不变，即每个块最多缓存一个副本
+ 完成后，您的输出应类似于下面显示的输出（但不完全相同）
+ 确保 usertests 仍然通过。 完成后， make grade应该通过所有测试
```S
$ bcachetest
start test0
test0 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 0 #acquire() 32954
lock: kmem: #fetch-and-add 0 #acquire() 75
lock: kmem: #fetch-and-add 0 #acquire() 73
lock: bcache: #fetch-and-add 0 #acquire() 85
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4159
lock: bcache.bucket: #fetch-and-add 0 #acquire() 2118
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4274
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4326
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6334
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6321
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6704
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6696
lock: bcache.bucket: #fetch-and-add 0 #acquire() 7757
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6199
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4136
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4136
lock: bcache.bucket: #fetch-and-add 0 #acquire() 2123
--- top 5 contended locks:
lock: virtio_disk: #fetch-and-add 158235 #acquire() 1193
lock: proc: #fetch-and-add 117563 #acquire() 3708493
lock: proc: #fetch-and-add 65921 #acquire() 3710254
lock: proc: #fetch-and-add 44090 #acquire() 3708607
lock: proc: #fetch-and-add 43252 #acquire() 3708521
tot= 128
test0: OK
start test1
test1 OK
$ usertests
  ...
ALL TESTS PASSED
$
```
+ 请为所有锁指定以“bcache”开头的名称
  + 也就是说，您应该 为每个锁 调用initlock ，并传递以“bcache”开头的名称
+ 减少块缓存中的争用比 kalloc 更棘手
  + 因为 bcache 缓冲区实际上是在进程（以及 CPU）之间共享的
  + 对于 kalloc，可以通过为每个 CPU 提供自己的分配器来消除大多数争用
    + 但这对于块缓存不起作用
    + 我们建议您使用每个哈希桶都有一个锁的哈希表在缓存中查找块编号
+ 在某些情况下，如果你的解决方案存在锁冲突，这是没问题的：
  + 当两个进程同时使用相同的块号时， bcachetest test0永远不会这样做
  + 当两个进程同时在缓存中丢失数据，并且需要找到一个未使用的块来替换时， bcachetest test0永远不会这样做
  + 当两个进程同时使用与您用于划分块和锁的任何方案相冲突的块时
    + 例如，如果两个进程使用的块的块号哈希到哈希表中的同一个插槽
    + bcachetest test0 可能会这样做，具体取决于您的设计
    + 但您应该尝试调整方案的细节以避免冲突（例如，更改哈希表的大小）。
+ bcachetest的test1使用的不同块比缓冲区还多，并且运用了大量的文件系统代码路径
## 官方提示
+ 阅读xv6书中关于块缓存的描述（第8.1-8.3节）
+ 可以使用固定数量的 bucket，并且不动态调整哈希表的大小
  + 使用素数的 bucket（例如 13）可以降低哈希冲突的可能性
+ 在哈希表中搜索缓冲区并在未找到缓冲区时为该缓冲区分配一个条目必须是原子的
+ 删除所有缓冲区的列表（bcache.head等），改为使用缓冲区上次使用的时间（即使用kernel/trap.c 中的ticks ）对缓冲区进行时间戳记
  + 通过此更改， brelse不需要获取 bcache 锁，而bget可以根据时间戳记选择最近最少使用的块
+ 在bget中序列化驱逐是可以的（即，当在缓存中查找失败时， bget的部分会选择一个缓冲区来重新使用）
+ 在某些情况下，您的解决方案可能需要持有两个锁
  + 例如，在驱逐期间，您可能需要持有 bcache 锁和每个 bucket 的锁。确保避免死锁
+ 替换块时，您可能会将struct buf从一个存储桶移动到另一个存储桶，因为新块的哈希值会移至另一个存储桶 
  + 您可能会遇到一种棘手的情况：
    + 新块的哈希值可能会与旧块移至同一个存储桶
  + 在这种情况下，请确保避免死锁
+ 一些调试技巧：
  + 实现 bucket lock，但将全局 bcache.lock 获取/释放保留在 bget 的开始/结束处以序列化代码
  + 一旦您确定它是正确的，没有竞争条件，请删除全局锁并处理并发问题
  + 您还可以运行make CPUS=1 qemu以使用一个核心进行测试
## 解题步骤
+  kalloc 中，一个物理页分配后就只归单个进程所管
+  但是在bcache 中，缓存块是会被多个进程（进一步地，被多个 CPU）共享的（由于多个进程可以同时访问同一个区块）
   +  所以 kmem 中为每个 CPU 预先分割一部分专属的页的方法在这里是行不通的
   +  所以需要降低锁的粒度，用更精细的锁 scheme 来降低出现竞争的概率
+  这里要提前说一下buf结构体：
```c
struct buf {
  int valid;         // 标记缓冲区中的数据是否已经从磁盘读取过来
  int disk;          // 标记该缓冲区是否由磁盘控制（即是否由磁盘管理该缓存块的内容）
  uint dev;          // 该缓冲区对应的设备号，表示磁盘设备
  uint blockno;      // 缓冲区中的数据块编号，通常表示在磁盘上的位置（即块号）
  
  struct sleeplock lock;  // 缓冲区的自旋锁，保护缓冲区在多个线程/进程间的访问，避免并发访问导致的错误
  uint refcnt;            // 缓冲区的引用计数，记录有多少地方正在使用该缓冲区
  struct buf *prev;       // 指向链表中上一个缓冲区的指针，用于实现LRU（Least Recently Used）缓存管理
  struct buf *next;       // 指向链表中下一个缓冲区的指针，用于实现LRU缓存管理
  
  uchar data[BSIZE];      // 存储数据的数组，用于存放从磁盘读取的数据或待写入的数据
};
```
### 第一步：定义哈希桶结构，并在bcache中删除全局缓冲区链表，改为使用素数个散列桶
+ 题目中的提示：
  + 使用固定数量的 bucket，并且不动态调整哈希表的大小
  + 使用素数的 bucket（例如 13）可以降低哈希冲突的可能性
```c
#define NBUCKET 13
#define HASH(id) (id%NBUCKET)

struct{
  struct buf head;      // 头节点
  struct spinlock lock; // lock
}hashbuf;

struct {
  struct buf buf[NBUF];     // 所有缓冲区的数组

// 所有缓冲区的链接列表，通过 prev/next 
// 按缓冲区的最近使用时间排序
// head.next 是最近的，head.prev 是最少的
  struct hashbuf buckets[NBUCKET];// 散列通数组，用于缓冲区的快速访问
} bcache;
```
### 第二步：修改binit
+ 初始化散列桶的锁
  + 为所有锁指定以“bcache”开头的名称
+ 将所有散列桶的head->prev、head->next都指向自身表示为空
+ 将所有的缓冲区挂载到bucket[0]桶上，代码如下
```c
void
binit(void)
{
  struct buf *b;                 // 用于遍历缓冲区数组的指针
  char lockname[16];             // 用于存储锁的名称（名字是散列桶的标识）

  // 初始化散列桶
  for(int i = 0; i < NBUCKET; i++){
    // 题目要求：为所有锁指定以“bcache”开头的名称
    snprintf(lockname, sizeof(lockname), "bcache_%d", i); 
    initlock(&bcache.buckets[i].lock, lockname);         

    // 初始化散列桶中的链表头，头节点的 prev 和 next 指针指向自己
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
  }

  // 初始化缓冲区数组并将其插入到第一个散列桶（桶索引为 0）的链表中
  for(b = bcache.buf; b < bcache.buf + NBUF; b++){
    // 利用头插法将缓冲区插入到散列桶 0 的链表头部
    b->next = bcache.buckets[0].head.next;  // b 的 next 指针指向原链表的第一个元素
    b->prev = &bcache.buckets[0].head;      // b 的 prev 指针指向链表的头节点

    // 初始化缓冲区的睡眠锁,防止多个线程同时访问和修改同一个缓冲区，导致数据不一致
    initsleeplock(&b->lock, "buffer");

    // 更新链表中其他节点的指针，插入 b 到链表头部
    bcache.buckets[0].head.next->prev = b; // 原先的第一个元素的 prev 指向 b
    bcache.buckets[0].head.next = b;       // 头节点的 next 指向 b
  }
}
```
### 第三步：在buf中加入时间戳
```c
struct buf {
  int valid;         // 标记缓冲区中的数据是否已经从磁盘读取过来
  int disk;          // 标记该缓冲区是否由磁盘控制（即是否由磁盘管理该缓存块的内容）
  uint dev;          // 该缓冲区对应的设备号，表示磁盘设备
  uint blockno;      // 缓冲区中的数据块编号，通常表示在磁盘上的位置（即块号）
  
  struct sleeplock lock;  // 缓冲区的自旋锁，保护缓冲区在多个线程/进程间的访问，避免并发访问导致的错误
  uint refcnt;            // 缓冲区的引用计数，记录有多少地方正在使用该缓冲区
  struct buf *prev;       // 指向链表中上一个缓冲区的指针，用于实现LRU（Least Recently Used）缓存管理
  struct buf *next;       // 指向链表中下一个缓冲区的指针，用于实现LRU缓存管理
  
  uchar data[BSIZE];      // 存储数据的数组，用于存放从磁盘读取的数据或待写入的数据
  uint timestamp;         // 时间戳
};
```
+ 这里反复阅读题目才理解这个时间戳有什么用：
  + 在原始方案中，每次brelse都将被释放的缓冲区挂载到链表头
    + 表明这个缓冲区最近刚刚被使用过
    + 在bget中分配时从链表尾向前查找，这样符合条件的第一个就是最久未使用的
  + 而在提示中建议使用时间戳作为LRU判定的法则
    + 这样我们就无需在brelse中进行头插法更改结点位置
### 第四步： 更改bget
+ 这个函数本来的作用就是遍历整个buffer cache找对应块的缓存
  + 如果找到，直接返回这个缓存块，把引用计数增加就行
  + 没找到的话，就需要先把当前使用最早的缓存块淘汰掉
+ 原版应该是把引用计数为0的块换掉
  + 这里实验多加了一条要求，就是要按照时间来判断，把最早的替换掉
  + 如果当前没找到满足要求的缓存块
    + 就需要从其他桶中找一个空闲的块分配给当前块使用
```c
static struct buf* 
bget(uint dev, uint blockno)
{
  struct buf *b;

  // 根据块号通过哈希函数获取所在的桶索引
  int bid = HASH(blockno);

  // 获取目标桶的锁，防止并发访问冲突
  acquire(&bcache.buckets[bid].lock);

  // 遍历该桶中的所有缓冲区，寻找目标块
  for(b = bcache.buckets[bid].head.next; b != &bcache.buckets[bid].head; b = b->next){
    // 如果找到块号和设备号都匹配的缓冲区
    if(b->dev == dev && b->blockno == blockno){
      // 增加引用计数
      b -> refcnt++;

      // 记录当前时间戳（系统时钟）
      acquire(&tickslock);
      b -> timestamp = ticks;
      release(&tickslock);

      // 释放桶的锁，接下来可以进行缓冲区的进一步操作
      release(&bcache.buckets[bid].lock);

      // 进入缓冲区的锁，确保只有一个线程可以操作该缓冲区
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 如果在当前桶中没有找到对应的缓冲区，设置空指针b
  b = 0;
  struct buf* tmp;

  // 遍历所有桶（最大循环次数为NBUCKET），尝试在其他桶中寻找合适的替换缓冲区
  for(int i = bid, cycle = 0; cycle != NBUCKET; i = (i + 1) % NBUCKET, cycle++){
    // 如果遍历到的桶是当前桶，则不重新获取锁
    if(i != bid){
      // 如果该桶的锁没有被持有，则获取锁
      if(!holding(&bcache.buckets[i].lock)){
        acquire(&bcache.buckets[i].lock);
      } else {
        // 如果当前线程已经持有该桶的锁，则跳过
        continue;
      }
    }

    // 遍历该桶的所有缓冲区，寻找一个空闲的且最久未使用的缓冲区进行替换
    for(tmp = bcache.buckets[i].head.next; tmp != &bcache.buckets[i].head; tmp = tmp->next){
      // 如果该缓冲区的引用计数为0且是最久未使用的缓冲区（LRU策略）
      if(tmp->refcnt == 0 && (b == 0 || tmp->timestamp < b->timestamp)){
        b = tmp;
      }
    }

    // 如果找到了合适的缓冲区b
    if(b){
      // 如果该缓冲区来自于其他桶，则需要将它从原桶中移除，并插入到当前桶
      if(i != bid){
        // 从当前桶中移除b缓冲区
        b->next->prev = b->prev;
        b->prev->next = b->next;

        // 释放当前桶的锁
        release(&bcache.buckets[i].lock);

        // 将b插入到目标桶的头部（头插法）
        b->next = bcache.buckets[bid].head.next;
        b->prev = &bcache.buckets[bid].head;
        bcache.buckets[bid].head.next->prev = b;
        bcache.buckets[bid].head.next = b;
      }

      // 设置缓冲区的设备号和块号
      b->dev = dev;
      b->blockno = blockno;
      // 标记该缓冲区无效
      b->valid = 0;
      // 重置引用计数为1（表示该缓冲区正在被使用）
      b->refcnt = 1;

      // 记录当前时间戳
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);

      // 释放目标桶的锁
      release(&bcache.buckets[bid].lock);

      // 获取该缓冲区的锁，进行后续的操作
      acquiresleep(&b->lock);
      return b;
    } else {
      // 如果在当前桶中没有找到合适的缓冲区，则释放该桶的锁
      if(i != bid){
        release(&bcache.buckets[i].lock);
      }
    }
  }

  return 0;
}
```
### 第五步：捎带着改一下别的函数
```c
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int bid = HASH(b->blockno);

  acquire(&bcache.buckets[bid].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.buckets[bid].head.next;
    b->prev = &bcache.buckets[bid].head;
    bcache.buckets[bid].head.next->prev = b;
    bcache.buckets[bid].head.next = b;
  }
  
  release(&bcache.buckets[bid].lock);
}

void
bpin(struct buf *b) {
  int bid = HASH(b->blockno);
  acquire(&bcache.buckets[bid].lock);
  b->refcnt++;
  release(&bcache.buckets[bid].lock);
}

void
bunpin(struct buf *b) {
  int bid = HASH(b->blockno);
  acquire(&bcache.buckets[bid].lock);
  b->refcnt--;
  release(&bcache.buckets[bid].lock);
}
```
## 最后结果
<img src=".\picture\image8.png">
<img src=".\picture\image7.png">






















































