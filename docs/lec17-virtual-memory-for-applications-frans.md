# 应用程序虚拟内存
## 应用程序使用虚拟内存所需要的特性
+ 用户应用程序本身就是运行在虚拟内存之上
  + 这里说的虚拟内存是指：User Mode或者应用程序想要使用与内核相同的机制，来产生Page Fault并响应Page Fault
  + 内核中几乎所有的虚拟内存技巧都基于Page Fault
+ 也就是说User Mode需要能够修改PTE的Protection位或者Privileged level
  + 注，Protection位是PTE中表明对当前Page的保护
+ 通过查看6-7种不同的应用程序，来说明用户应用程序使用虚拟内存的必要性
  + 这些应用程序包括了：
    + Garbage Collector
    + Data Compression Application
    + Shared Virtual Memory
----------------------------------------------------------
第一个问题是，上面的应用程序需要的特性是什么？
+ 首先，需要trap来使得发生在内核中的Page Fault可以传播到用户空间
  + 然后在用户空间的handler可以处理相应的Page Fault
  + 之后再以正常的方式返回到内核并恢复指令的执行
    + 这个特性是必须的，否则的话，不能基于Page Fault做任何事情
+ 第二个特性是Prot1，它会降低了一个内存Page的accessability
  + accessability的意思是指内存Page的读写权限
  + 内存Page的accessability有不同的降低方式
  + 例如，将一个可以读写的Page变成只读的，或者将一个只读的Page变成完全没有权限
+ 除了对于每个内存Page的Prot1，还有管理多个Page的ProtN
  + ProtN基本上等效于调用N次Prot1，那为什么还需要有ProtN
    + 因为单次ProtN的损耗比Prot1大不了多少，使用ProtN可以将成本分摊到N个Page，使得操作单个Page的性能损耗更少
    + 在使用Prot1时，你需要修改PTE的bit位，并且在Prot1的结束时，需要清除TLB
      + 而清除TLB比较费时。如果能对所有需要修改的内存Page集中清理一次TLB，就可以将成本分摊
    + 所以**ProtN等效于修改PTE的bit位N次，再加上清除一次TLB**
+ 下一个特性是Unprot
  + 它增加了内存Page的accessability
  + 例如将本来只读的Page变成可读可写的
+ 除此之外，还需要能够查看内存Page是否是Dirty
+ 以及map2
  + map2使得一个应用程序可以将一个特定的内存地址空间映射两次
  + 并且这两次映射拥有不同的accessability
    + （注，也就是一段物理内存对应两份虚拟内存，并且两份虚拟内存有不同的accessability）
## 支持应用程序使用虚拟内存的系统调用
+ 第一个或许也是最重要的一个，是一个叫做mmap的系统调用
  + 它接收某个对象，并将其映射到调用者的地址空间中
    + 举个例子，如果你想映射一个文件，那么你需要将文件描述符传递给mmap系统调用
  + mmap系统调用有许多令人眼花缭乱的参数（注，mmap的具体说明可以参考man page）:
    + 第一个参数是一个你想映射到的特定地址
      + 如果传入null表示不指定特定地址，这样的话内核会选择一个地址来完成映射，并从系统调用返回
    + 第二个参数是想要映射的地址段长度len
    + 第三个参数是Protection bit
      + 例如读写R|W
    + 第四个参数是flags，它的值可以是MAP_PRIVATE
      + 在mmap文件的场景下，MAP_PRIVATE表明更新文件不会写入磁盘，只会更新在内存中的拷贝，详见man page
    + 第五个参数是传入的对象，在上面的例子中就是文件描述符
    + 第六个参数是offset
+ 通过上面的系统调用
  + 可以将文件描述符指向的文件内容
  + 从起始位置加上offset的地方开始
  + 映射到特定的内存地址（如果指定了的话）
  + 并且连续映射len长度
    + 这使得可以实现Memory Mapped File
    + 可以将文件的内容带到内存地址空间
    + 进而只需要方便的通过普通的指针操作
    + 而不用调用read/write系统调用
    + 就可以从磁盘读写文件内容。这是一个方便的接口，可以用来操纵存储在文件中的数据结构。实际上，你们将会在下个lab实现基于文件的mmap，下个lab结合了XV6的文件系统和虚拟内存，进而实现mmap。
+ mmap还可以用作他途
  + 除了可以映射文件之外，还可以用来映射匿名的内存（Anonymous Memory）
  + 这是sbrk（注，详见8.2）的替代方案
    + 可以向内核申请物理内存，然后映射到特定的虚拟内存地址




























































































































































































