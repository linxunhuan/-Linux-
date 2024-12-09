# 文件系统性能和快速崩溃恢复
## logging
+ XV6 File system logging
  + 磁盘分为了两个部分：
    + 首先是文件系统目录的树结构，以root目录为根节点，往下可能有其他的目录
      + 我们可以认为目录结构就是一个树状的数据结构
      + 假设root目录下有两个子目录D1和D2
        + D1目录下有两个文件F1和F2
        + 每个文件又包含了一些block
    + 除此之外，还有一些其他并非是树状结构的数据
      + 比如bitmap表明了每一个data block是空闲的还是已经被分配了
      + inode，目录内容
      + bitmap block，我们将会称之为metadata block
        + （注，Frans和Robert在这里可能有些概念不统一，对于Frans来说，目录内容应该也属于文件内容，目录是一种特殊的文件，详见14.3；而对于Robert来说，目录内容是metadata。）
    + 另一类就是持有了文件内容的block，或者叫data block
+ 除了文件系统之外，XV6在磁盘最开始的位置还有一段log
  + XV6的log相对来说比较简单，它有header block
  + 之后是一些包含了有变更的文件系统block
  + 这里可以是metadata block也可以是data block
  + header block会记录之后的每一个log block应该属于文件系统中哪个block
    + 假设第一个log block属于block 17
    + 第二个属于block 29
+ 在计算机上，我们会有一些用户程序调用write/create系统调用来修改文件系统
  + 在内核中存在block cache
    + 最初write请求会被发到block cache
  + block cache就是磁盘中block在内存中的拷贝，所以最初对于文件block或者inode的更新走到了block cache
+ 在write系统调用的最后，这些更新都被拷贝到了log中
  + 之后我们会更新header block的计数来表明当前的transaction已经结束了
  + 在文件系统的代码中，任何修改了文件系统的系统调用函数中，某个位置会有begin_op
    + 表明马上就要进行一系列对于文件系统的更新了，不过在完成所有的更新之前，不要执行任何一个更新
  + 在begin_op之后是一系列的read/write操作
  + 最后是end_op
    + 用来告诉文件系统现在已经完成了所有write操作
  + 所以在begin_op和end_op之间，所有的write block操作只会走到block cache中
  + 当系统调用走到了end_op函数，文件系统会将修改过的block cache拷贝到log中
+ 在拷贝完成之后，文件系统会将修改过的block数量
  + 通过一个磁盘写操作写入到log的header block
  + 这次写入被称为commit point
+ 在commit point之前，如果发生了crash
  + 在重启时，整个transaction的所有写磁盘操作最后都不会应用
  + 在commit point之后，即使立即发生了crash，重启时恢复软件会发现在log header中记录的修改过的block数量不为0
  + 接下来就会将log header中记录的所有block，从log区域写入到文件系统区域
+ 这里实际上使得系统调用中位于begin_op和end_op之间的所有写操作在面对crash时具备原子性
  + 也就是说，要么文件系统在crash之前更新了log的header block
    + 这样所有的写操作都能生效
  + 要么crash发生在文件系统更新log的header block之前
    + 这样没有一个写操作能生效
+ 在crash并重启时，必须有一些恢复软件能读取log的header block，并判断里面是否记录了未被应用的block编号
  + 如果有的话，需要写（也有可能是重写）log block到文件系统中对应的位置
  + 如果没有的话，恢复软件什么也不用做
+ **这里有几个超级重要的点，不仅针对XV6，对于大部分logging系统都适用**：
  + 包括XV6在内的所有logging系统，都需要遵守write ahead rule
    + 这里的意思是，任何时候如果一堆写操作需要具备原子性，系统需要先将所有的写操作记录在log中,之后才能将这些写操作应用到文件系统的实际位置
    + 也就是说，我们需要预先在log中定义好所有需要具备原子性的更新，之后才能应用这些更新
    + write ahead rule是logging能实现故障恢复的基础
    + write ahead rule使得一系列的更新在面对crash时具备了原子性
  + 另一点是，XV6对于不同的系统调用复用的是同一段log空间，但是直到log中所有的写操作被更新到文件系统之前，我们都不能释放或者重用log
    + 它表明我们不能覆盖或者重用log空间，直到保存了transaction所有更新的这段log，都已经反应在了文件系统中
+ 所以在XV6中，end_op做了大量的工作
  + 首先是将所有的block记录在log中，之后是更新log header
  + 在没有crash的正常情况，文件系统需要再次将所有的block写入到磁盘的文件系统中
    + 磁盘中的文件系统更新完成之后，XV6文件系统还需要删除header block记录的变更了的block数量，以表明transaction已经完成了，之后就可以重用log空间
+ 在向log写入任何新内容之前，删除header block中记录的block数量也很重要
  + 因为你不会想要在header block中记录的还是前一个transaction的信息，而log中记录的又是一个新的transaction的数据
  + 可以假设新的transaction对应的是与之前不同的block编号的数据
    + 这样的话，在crash重启时，log中的数据会被写入到之前记录的旧的block编号位置
    + 所以我们必须要先清除header block
+ freeing rule的意思就是
  + 在从log中删除一个transaction之前，我们必须将所有log中的所有block都写到文件系统中
------------------------------
+ 这些规则使得
  + 就算一个文件系统更新可能会复杂且包含多个写操作，但是每次更新都是原子的
    + 在crash并重启之后，要么所有的写操作都生效，要么没有写操作能生效
+ 要介绍Linux的logging方案，就需要了解XV6的logging有什么问题？
+ 为什么Linux不使用与XV6完全一样的logging方案？
  + 这里的回答简单来说就是XV6的logging太慢了
  + XV6中的任何一个例如create/write的系统调用，需要在整个transaction完成之后才能返回
  + 所以在创建文件的系统调用返回到用户空间之前，它需要完成所有end_op包含的内容，这包括了：
    + 将所有更新了的block写入到log
    + 更新header block
    + 将log中的所有block写回到文件系统分区中
    + 清除header block
  + 在任何一个文件系统调用的commit过程中，不仅是占据了大量的时间，而且其他系统调用也不能对文件系统有任何的更新
  + 所以这里的系统调用实际上是一次一个的发生，而每个系统调用需要许多个写磁盘的操作
  + 这里每个系统调用需要等待它包含的所有写磁盘结束，对应的技术术语被称为synchronize
    + XV6的系统调用对于写磁盘操作来说是同步的（synchronized），所以它非常非常的慢
    + 在使用机械硬盘时，它出奇的慢
      + 因为每个写磁盘都需要花费10毫秒，而每个系统调用又包含了多个写磁盘操作
      + 所以XV6每秒只能完成几个更改文件系统的系统调用
+ 另一件需要注意的更具体的事情是，在XV6的logging方案中，每个block都被写了两次
  + 第一次写入到了log
  + 第二次才写入到实际的位置
  + 虽然这么做有它的原因，但是ext3可以一定程度上修复这个问题
## ext3 文件系统日志格式
+ ext3的数据结构与XV6是类似的
  + 在内存中，存在block cache
    + 这是一种write-back cache
    + （注，区别于write-through cache，指的是cache稍后才会同步到真正的后端）
  + block cache中缓存了一些block
    + 其中的一些是干净的数据，因为它们与磁盘上的数据是一致的
    + 其他一些是脏数据，因为从磁盘读出来之后被修改过
    + 有一些被固定在cache中，基于前面介绍的write-ahead rule和freeing rule，不被允许写回到磁盘中
+ 除此之外，ext3还维护了一些transaction信息
  + 它可以维护多个在不同阶段的transaction的信息
  + 每个transaction的信息包含有：
    + 一个序列号
    + 一系列该transaction修改的block编号
      + 这些block编号指向的是在cache中的block，因为任何修改最初都是在cache中完成
    + 以及一系列的handle
      + handle对应了系统调用
      + 并且这些系统调用是transaction的一部分，会读写cache中的block
+ 在磁盘上，与XV6一样：
  + 会有一个文件系统树，包含了inode，目录，文件等等
  + 会有bitmap block来表明每个data block是被分配的还是空闲的
  + 在磁盘的一个指定区域，会保存log
+ 目前为止，这与XV6非常相似
  + 主要的区别在于ext3可以同时跟踪多个在不同执行阶段的transaction
+ 接下来我们详细看一下ext3的log中有什么，这与XV6中的log有点不一样
  + 在log的最开始，是super block
    + 这是log的super block，而不是文件系统的super block
  + log的super block包含了log中第一个有效的transaction的起始位置和序列号
    + 起始位置就是磁盘上log分区的block编号
    + 序列号就是前面提到的每个transaction都有的序列号
  + log是磁盘上一段固定大小的连续的block
  + log中，除了super block以外的block存储了transaction。每个transaction在log中包含了：
    + 一个descriptor block
      + 其中包含了log数据对应的实际block编号
      + 这与XV6中的header block很像
    + 之后是针对每一个block编号的更新数据
    + 最后当一个transaction完成并commit了，会有一个commit block
+ 因为log中可能有多个transaction
  + commit block之后可能会跟着下一个transaction的descriptor block，data block和commit block
  + 所以log可能会很长并包含多个transaction
  + 我们可以认为super block中的起始位置和序列号属于最早的，排名最靠前的，并且是有效的transaction
+ 在crash之后的恢复过程会扫描log
  + 为了将descriptor block和commit block与data block区分开
    + descriptor block和commit block会以一个32bit的魔法数字作为起始
    + 这个魔法数字不太可能出现在数据中，并且可以帮助恢复软件区分不同的block
## ext3如何提升性能
+ ext3通过3种方式提升了性能：
  + 首先，它提供了异步的（asynchronous）系统调用
    + 也就是说系统调用在写入到磁盘之前就返回了，系统调用只会更新缓存在内存中的block，并不用等待写磁盘操作
    + 不过它可能会等待读磁盘
  + 第二，它提供了批量执行（batching）的能力
    + 可以将多个系统调用打包成一个transaction
  + 最后，它提供了并发（concurrency）
### 异步的系统调用
+ 这表示系统调用修改完位于缓存中的block之后就返回，并不会触发写磁盘
  + 所以这里明显的优势就是系统调用能够快速的返回
+ 同时它也使得I/O可以并行的运行
  + 也就是说应用程序可以调用一些文件系统的系统调用
  + 但是应用程序可以很快从系统调用中返回并继续运算
  + 与此同时文件系统在后台会并行的完成之前的系统调用所要求的写磁盘操作
+ 这被称为I/O concurrency
  + 如果没有异步系统调用，很难获得I/O concurrency
  + 或者说很难同时进行磁盘操作和应用程序运算
    + 因为同步系统调用中，应用程序总是要等待磁盘操作结束才能从系统调用中返回
+ 另一个异步系统调用带来的好处是，它使得大量的批量执行变得容易
----------------------------------
+ 异步系统调用的缺点:
  + 系统调用的返回并不能表示系统调用应该完成的工作实际完成了
  + 举个例子
    + 如果你创建了一个文件并写了一些数据然后关闭文件并在console向用户输出done
    + 最后你把电脑的电给断了
    + 尽管所有的系统调用都完成了，程序也输出了done，但是在你重启之后，你的数据并不一定存在
      + 这意味着，在异步系统调用的世界里，如果应用程序关心可能发生的crash，那么应用程序代码应该更加的小心
    + 这在XV6并不是什么大事
      + 因为如果XV6中的write返回了，那么数据就在磁盘上，crash之后也还在
    + 而ext3中，如果write返回了，你完全不能确定crash之后数据还在不在
    + 所以一些应用程序的代码应该仔细编写
      + 例如对于数据库，对于文本编辑器
      + 如果写了一个文件，我不想在我写文件过程断电然后再重启之后看到的是垃圾文件或者不完整的文件，我想看到的要么是旧的文件，要么是新的文件
+ 所以文件系统对于这类应用程序也提供了一些工具以确保在crash之后可以有预期的结果
  + 这里的工具是一个系统调用，叫做fsync，所有的UNIX都有这个系统调用
  + 这个系统调用接收一个文件描述符作为参数
    + 它会告诉文件系统去完成所有的与该文件相关的写磁盘操作
    + 在所有的数据都确认写入到磁盘之后，fsync才会返回
  + 如果查看数据库，文本编辑器或者一些非常关心文件数据的应用程序的源代码
    + 将会看到精心放置的对于fsync的调用
    + fsync可以帮助解决异步系统调用的问题
  + 对于大部分程序，例如编译器，如果crash了编译器的输出丢失了其实没什么
    + 所以许多程序并不会调用fsync
    + 并且乐于获得异步系统调用带来的高性能
### 批量执行（batching）
+ 在任何时候，ext3只会有一个open transaction
+ ext3中的一个transaction可以包含多个不同的系统调用
+ 所以ext3是这么工作的：
  + 它首先会宣告要开始一个新的transaction
    + 接下来的几秒所有的系统调用都是这个大的transaction的一部分
+ 我认为默认情况下，ext3每5秒钟都会创建一个新的transaction
  + 所以每个transaction都会包含5秒钟内的系统调用
  + 这些系统调用都打包在一个transaction中
  + 在5秒钟结束的时候，ext3会commit这个包含了可能有数百个更新的大transaction
------------------------
+ 优点：
  + 首先它在多个系统调用之间分摊了transaction带来的固有的损耗
    + 固有的损耗包括写transaction的descriptor block和commit block
    + 在一个机械硬盘中需要查找log的位置并等待磁碟旋转，这些都是成本很高的操作，现在只需要对一批系统调用执行一次，而不用对每个系统调用执行一次这些操作，所以batching可以降低这些损耗带来的影响
  + 另外，它可以更容易触发write absorption
    + 经常会有这样的情况，你有一堆系统调用最终在反复更新相同的一组磁盘block
    + 举个例子，如果我创建了一些文件，我需要分配一些inode
      + inode或许都很小只有64个字节，一个block包含了很多个inode
        + 所以同时创建一堆文件只会影响几个block的数据
      + 类似的，如果我向一个文件写一堆数据
        + 我需要申请大量的data block
        + 我需要修改表示block空闲状态的bitmap block中的很多个bit位
        + 如果我分配到的是相邻的data block，它们对应的bit会在同一个bitmap block中
        + 所以我可能只是修改一个block的很多个bit位
        + 所以一堆系统调用可能会反复更新一组相同的磁盘block
        + 通过batching，多次更新同一组block会先快速的在内存的block cache中完成，之后在transaction结束时，一次性的写入磁盘的log中
        + 这被称为write absorption，相比一个类似于XV6的同步文件系统，它可以极大的减少写磁盘的总时间
### disk scheduling
+ 假设我们要向磁盘写1000个block，不论是在机械硬盘还是SSD（机械硬盘效果会更好）
  + 一次性的向磁盘的连续位置写入1000个block，要比分1000次每次写一个不同位置的磁盘block快得多
  + 我们写log就是向磁盘的连续位置写block
    + 通过向磁盘提交大批量的写操作，可以更加的高效
    + 这里我们不仅通过向log中连续位置写入大量block来获得更高的效率，甚至当我们向文件系统分区写入包含在一个大的transaction中的多个更新时，如果我们能将大量的写请求同时发送到驱动
    + 即使它们位于磁盘的不同位置，我们也使得磁盘可以调度这些写请求，并以特定的顺序执行这些写请求
    + 在一个机械硬盘上，如果一次发送大量需要更新block的写请求，驱动可以对这些写请求根据轨道号排序
    + 甚至在一个固态硬盘中，通过一次发送给硬盘大量的更新操作也可以稍微提升性能
    + 所以，只有发送给驱动大量的写操作，才有可能获得disk scheduling
### concurrency
+ 首先ext3允许多个系统调用同时执行，所以我们可以有并行执行的多个不同的系统调用
+ 在ext3决定关闭并commit当前的transaction之前，系统调用不必等待其他的系统调用完成
+ 它可以直接修改作为transaction一部分的block
  + 许多个系统调用都可以并行的执行，并向当前transaction增加block
  + 这在一个多核计算机上尤其重要，因为我们不会想要其他的CPU核在等待锁
    + 在XV6中，如果当前的transaction还没有完成，新的系统调用不能继续执行
    + 在ext3中，大多数时候多个系统调用都可以更改当前正在进行的transaction
+ 另一种ext3提供的并发是
  + 可以有多个不同状态的transaction同时存在
    + 所以尽管只有一个open transaction可以接收系统调用，但是其他之前的transaction可以并行的写磁盘
    + 这里可以并行存在的不同transaction状态包括了：
      + 首先是一个open transaction
      + 若干个正在commit到log的transaction，我们并不需要等待这些transaction结束
        + 当之前的transaction还没有commit并还在写log的过程中，新的系统调用仍然可以在当前的open transaction中进行
      + 若干个正在从cache中向文件系统block写数据的transaction
      + 若干个正在被释放的transaction，这个并不占用太多的工作
+ 通常来说会有位于不同阶段的多个transaction，新的系统调用不必等待旧的transaction提交到log或者写入到文件系统
+ 对比之下，XV6中新的系统调用就需要等待前一个transaction完全完成
+ concurrency之所以能帮助提升性能，是因为它可以帮助我们并行的运行系统调用，我们可以得到多核的并行能力
  + 如果我们可以在运行应用程序和系统调用的同时，来写磁盘，我们可以得到I/O concurrency，也就是同时运行CPU和磁盘I/O
  + 这些都能帮助我们更有效，更精细的使用硬件资源
## ext3文件系统调用格式
+ 在Linux的文件系统中，我们需要每个系统调用都声明一系列写操作的开始和结束
+ 实际上在任何transaction系统中，都需要明确的表示开始和结束，这样之间的所有内容都是原子的
  + 所以系统调用中会调用start函数
+ ext3需要知道当前正在进行的系统调用个数
  + 所以每个系统调用在调用了start函数之后，会得到一个handle，它某种程度上唯一识别了当前系统调用
  + 当前系统调用的所有写操作都是通过这个handle来识别跟踪的
+ 之后系统调用需要读写block
  + 它可以通过get获取block在buffer中的缓存
    + 同时告诉handle这个block需要被读或者被写
  + 如果需要更改多个block，类似的操作可能会执行多次
+ 之后是修改位于缓存中的block
  + 当这个系统调用结束时，它会调用stop函数，并将handle作为参数传入
+ 除非transaction中所有已经开始的系统调用都完成了，transaction是不能commit的
  + 因为可能有多个transaction，文件系统需要有种方式能够记住系统调用属于哪个transaction
    + 这样当系统调用结束时，文件系统就知道这是哪个transaction正在等待的系统调用
    + 所以handle需要作为参数传递给stop函数
      + 因为每个transaction都有一堆block与之关联，修改这些block就是transaction的一部分内容
      + 所以我们将handle作为参数传递给get函数是为了告诉logging系统
        + 这个block是handle对应的transaction的一部分
+ stop函数并不会导致transaction的commit
  + 它只是告诉logging系统，当前的transaction少了一个正在进行的系统调用
+ transaction只能在所有已经开始了的系统调用都执行了stop之后才能commit
  + 所以transaction需要记住所有已经开始了的handle
  + 这样才能在系统调用结束的时候做好记录
## ext3 transaction commit步骤
+ commit transaction完整的步骤
  + 每隔5秒，文件系统都会commit当前的open transaction，下面是commit transaction涉及到的步骤：
    + 首先需要阻止新的系统调用
      + 当我们正在commit一个transaction时，我们不会想要有新增的系统调用
      + 我们只会想要包含已经开始了的系统调用，所以我们需要阻止新的系统调用
      + 这实际上会损害性能，因为在这段时间内系统调用需要等待并且不能执行
    + 第二，需要等待包含在transaction中的已经开始了的系统调用们结束
      + 所以我们需要等待transaction中未完成的系统调用完成
      + 这样transaction能够反映所有的写操作
    + 一旦transaction中的所有系统调用都完成了，也就是完成了更新cache中的数据
      + 那么就可以开始一个新的transaction
      + 并且让在第一步中等待的系统调用继续执行
      + 所以现在需要为后续的系统调用开始一个新的transaction
    + 更新descriptor block，其中包含了所有在transaction中被修改了的block编号
      + 系统调用在调用get函数时都将handle作为参数传入
      + 表明了block对应哪个transaction
    + 将被修改了的block，从缓存中写入到磁盘的log中
      + 在这个阶段，我们写入到磁盘log中的是transaction结束时，对于相关block cache的拷贝
      + 所以这一阶段是将实际的block写入到log中
    + 接下来，我们需要等待前两步中的写log结束
    + 之后我们可以写入commit block
    + 接下来我们需要等待写commit block结束
      + 结束之后，从技术上来说，当前transaction已经到达了commit point
      + 也就是说transaction中的写操作可以保证在面对crash并重启时还是可见的
      + 如果crash发生在写commit block之前，那么transaction中的写操作在crash并重启时会丢失
    + 接下来我们可以将transaction包含的block写入到文件系统中的实际位置
    + 在上一步中的所有写操作完成之后，我们才能重用transaction对应的那部分log空间
+ 在一个非常繁忙的系统中，log的头指针一直追着尾指针在跑
  + 也就是说一直没有新的log空间
  + 在当前最早的transaction的所有步骤都完成之前，或许不能开始commit一个新的transaction
    + 因为我们需要重复利用最早的transaction对应的log空间
  + 不过人们通常会将log设置的足够大，让这种情况就不太可能发生
## ext3 file system恢复过程
+ 当决定释放某段log空间时
  + 文件系统会更新super block中的指针将其指向当前最早的transaction的起始位置
+ 之后如果crash并重启，恢复软件会读取super block，并找到log的起始位置
  + 所以如果crash了，内存中的所有数据都会消失
    + 例如文件系统中记录的哪些block被写入到了磁盘中这些信息都会丢失
  + 所以可以假设这时内存中没有可用的数据，唯一可用的数据存在于磁盘中

## 总结
+ log是为了保证多个步骤的写磁盘操作具备原子性
  + 在发生crash时，要么这些写操作都发生，要么都不发生
  + 这是logging的主要作用
+ logging的正确性由write ahead rule来保证
  + 你们将会在故障恢复相关的业务中经常看到write ahead rule或者write ahead log（WAL）
  + write ahead rule的意思是
    + 你必须在做任何实际修改之前，将所有的更新commit到log中
    + 在稍后的恢复过程中完全依赖write ahead rule
  + 对于文件系统来说，logging的意义在于简单的快速恢复
    + log中可能包含了数百个block，你可以在一秒中之内重新执行这数百个block，不管你的文件系统有多大，之后又能正常使用了
+ 最后有关ext3的一个细节点是
  + 它使用了批量执行和并发来获得可观的性能提升，不过同时也带来了可观的复杂性的提升





















































































































































