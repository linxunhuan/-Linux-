# mmap
## 官方题目
+ mmap和munmap系统调用允许 UNIX 程序对其地址空间施加详细控制
+ 它们可用于在进程之间共享内存、将文件映射到进程地址空间，以及作为用户级页面错误方案（如讲座中讨论的垃圾收集算法）的一部分
+ 在本实验中，您将向 xv6 添加mmap和munmap ，重点关注内存映射文件
--------------------------
+ 手册页（运行 man 2 mmap）显示了 mmap 的以下声明：
```c
void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
```
+ mmap 可以通过多种方式调用，但本实验只需要与文件内存映射相关的部分功能
+ 您可以假设 addr 始终为零，这意味着内核应决定要映射文件的虚拟地址
+ mmap 返回该地址，如果失败则返回 0xffffffffffffffff
  + length 是映射的字节数
    + 它可能与文件的长度不同
  + prot 表示内存是否应映射为可读、可写和/或可执行
    + 您可以假设 prot 为 PROT_READ 或 PROT_WRITE 或两者
  + flags 将是 MAP_SHARED
    + 这意味着对映射内存的修改应写回文件，或 MAP_PRIVATE，这意味着它们不应该
  + 您不必在 flags 中实现任何其他位
  + fd 是要映射的文件的打开文件描述符
  + 您可以假设 offset 为零（它是文件中要映射的起点）
   + 如果映射同一个 MAP_SHARED 文件的进程不共享物理页面的话，那就没问题
+ munmap(addr, length) 应删除指定地址范围内的 mmap 映射
  + 如果进程已修改内存并将其映射到 MAP_SHARED，则应首先将修改写入文件
  + munmap 调用可能仅覆盖 mmap 区域的一部分
    + 但您可以假设它将在开始处、结束处或整个区域取消映射（但不会在区域中间打洞）
+ 您应该实现足够的 mmap 和 munmap 功能以使 mmaptest 测试程序正常工作
  + 如果 mmaptest 不使用 mmap 功能，则您无需实现该功能
## 官方提示
+ 首先将_mmaptest添加到UPROGS以及mmap和munmap系统调用，以便编译user/mmaptest.c、
  + 目前，只需从mmap和munmap返回错误
  + 我们 在kernel/fcntl.h中为您定义了PROT_READ等
  + 运行mmaptest，它将在第一次 mmap 调用时失败
+ 延迟填充页表，以响应页面错误
  + 也就是说，mmap不应分配物理内存或读取文件
  + 相反，应在usertrap中的页面错误处理代码（或由 usertrap 调用的代码）中执行此操作
    + 如在延迟页面分配实验室中一样
    + 延迟的原因是为了确保大文件的mmap速度快，并且大于物理内存的文件的mmap是可能的
+ 跟踪mmap为每个进程映射的内容
  + 定义一个与第 15 讲中描述的 VMA（虚拟内存区域）相对应的结构
  + 记录mmap创建的虚拟内存范围的地址、长度、权限、文件等
  + 由于 xv6 内核中没有内存分配器
    + 因此可以声明一个固定大小的 VMA 数组并根据需要从该数组中分配
  + 16 的大小应该足够了
+ 实现mmap：
  + 在进程的地址空间中找到一个未使用的区域来映射文件，并将 VMA 添加到进程的映射区域表
    + VMA 应包含指向被映射文件的结构文件的指针
    + mmap应增加文件的引用计数，以便结构不会在文件关闭时消失（提示：请参阅filedup）
  + 运行mmaptest：
    + 第一个mmap应该会成功
    + 但第一次访问经过 mmap 的内存将导致页面错误并终止mmaptest
+ 添加代码以在 mmap 区域中引发页面错误，从而分配一页物理内存
  + 将相关文件的 4096 个字节读入该页面，并将其映射到用户地址空间
  + 使用readi读取文件，它接受一个偏移量参数，以在该偏移量处读取文件（但您必须锁定/解锁传递给readi 的inode ）
  + 不要忘记在页面上正确设置权限
  + 运行mmaptest；它应该到达第一个munmap
+ 实现munmap：
  + 找到地址范围的 VMA 并取消映射指定的页面（提示：使用uvmunmap）
  + 如果munmap删除了前一个mmap的所有页面，它应该减少相应struct file的引用计数
  + 如果未映射的页面已被修改且文件被映射为MAP_SHARED，则将页面写回文件
  + 查看filewrite以获得灵感
+ 理想情况下，您的实现只会写回 程序实际修改的MAP_SHARED页面
  + RISC -V PTE 中的脏位 ( D ) 表示页面是否已被写入
  + 但是， mmaptest不会检查非脏页面是否未被写回
  + 因此，您可以不查看D位而直接写回页面
+ 修改exit以取消映射进程的映射区域，就像调用了munmap一样
  + 运行mmaptest
    + mmap_test应该会通过，但fork_test可能不会通过
+ 修改fork以确保子进程具有与父进程相同的映射区域
  + 不要忘记增加 VMA 的struct file的引用计数
  + 在子进程的页面错误处理程序中，可以分配一个新的物理页面
    + 而不是与父进程共享一个页面
  + 后者会更酷，但需要更多的实现工作
  + 运行mmaptest；它应该通过mmap_test和fork_test


















