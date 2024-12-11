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
+ 请为所有锁指定以“bcache”开头的名称。也就是说，您应该 为每个锁 调用initlock ，并传递以“bcache”开头的名称。

减少块缓存中的争用比 kalloc 更棘手，因为 bcache 缓冲区实际上是在进程（以及 CPU）之间共享的。对于 kalloc，可以通过为每个 CPU 提供自己的分配器来消除大多数争用；但这对于块缓存不起作用。我们建议您使用每个哈希桶都有一个锁的哈希表在缓存中查找块编号。

在某些情况下，如果你的解决方案存在锁冲突，这是没问题的：

当两个进程同时使用相同的块号时， bcachetest test0永远不会这样做。
当两个进程同时在缓存中丢失数据，并且需要找到一个未使用的块来替换时， bcachetest test0永远不会这样做。
当两个进程同时使用与您用于划分块和锁的任何方案相冲突的块时；例如，如果两个进程使用的块的块号哈希到哈希表中的同一个插槽。bcachetest test0 可能会这样做，具体取决于您的设计，但您应该尝试调整方案的细节以避免冲突（例如，更改哈希表的大小）。
bcachetest的test1使用的不同块比缓冲区还多，并且运用了大量的文件系统代码路径。
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































































