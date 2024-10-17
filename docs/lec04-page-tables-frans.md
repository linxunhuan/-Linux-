# 地址空间
+ 创造虚拟内存的目的——通过它实现隔离性
  + 每个用户程序都被装进一个盒子里，这样它们就不会彼此影响了
  + 让它们与内核操作系统相互独立，这样如果某个应用程序无意或者故意做了一些坏事，也不会影响到操作系统
+ 上述问题，一种实现方式是——内存空间
  + 给包括内核在内的所有程序专属的地址空间
  + 每个程序都运行在自己的地址空间，并且这些地址空间彼此之间相互独立

# 页表
+ 问题来了，如何实现地址空间？——页表
  + 在硬件中通过处理器和内存管理单元
+ 对于任何一条带有地址的指令，其中的地址应该认为是虚拟内存地址而不是物理地址
  + 假设寄存器a0中是地址0x1000，那么这是一个虚拟内存地址
  + 虚拟内存地址会被转到内存管理单元（MMU，Memory Management Unit）
  + 内存管理单元会将虚拟地址翻译成物理地址
    + 之后这个物理地址会被用来索引物理内存，并从物理内存加载，或者向物理内存存储数据
<img src=".\picture\image5.png">

+ 从CPU的角度来说，一旦MMU打开了，它执行的每条指令中的地址都是虚拟内存地址
+ 理想状态：
  + 每个应用程序都有自己独立的表单
    + 这个表单定义了应用程序的地址空间
  + 当操作系统将CPU从一个应用程序切换到另一个应用程序时，
    + 也需要切换SATP寄存器中的内容
    + 从而指向新的进程保存在物理内存中的地址对应表单
  + 这样的话，cat程序和Shell程序中相同的虚拟内存地址，就可以翻译到不同的物理内存地址，因为每个应用程序都有属于自己的不同的地址对应表单
+ 这样做最大的问题是——表单会变得非常大，内存全被耗尽了，所以真实情况：
  + 虚拟内存地址，划分为两个部分：
    + index
      + MMU读取虚拟内存地址中的index可以知道物理内存中的page号
      + page号对应了物理内存中的4096个字节
    + offset
      + offset指向了page中的4096个字节中的某一个
      + 假设offset是12，那么page中的第12个字节被使用了
      + 将offset加上page的起始地址，就可以得到物理内存地址
  + 虚拟内存地址都是64bit
    + 27bit做index
    + 12bit做offset
      + offset必须是12bit，因为对应了一个page的4096个字节
  + 物理内存地址是56bit
    + 44bit是物理page号（PPN，Physical Page Number）
    + 12bit是offset完全继承自虚拟内存地址
      + 也就是地址转换时
      + 将虚拟内存中的27bit翻译成物理内存中的44bit的page号，剩下的12bit offset直接拷贝过来即可

+ 下面是一个逐级嵌套的表：
  + 虚拟内存地址中的27bit的index
    + 由3个9bit的数字组成（L2，L1，L0）
    + 前9个bit被用来索引最高级的page directory
      + 一个directory是4096Bytes
        + 跟page的大小是一样的
      + Directory中的一个条目被称为PTE（Page Table Entry）是64bits
        + 和寄存器的大小一样，也就是8Bytes
      + 所以一个Directory page有512个条目
  + 寄存器会指向最高一级的page directory的物理内存地址
    + 之后用虚拟内存中index的高9bit用来索引最高一级的page directory
    + 这样就能得到一个PPN，也就是物理page号
    + 这个PPN指向了中间级的page directory
  + 在使用中间级的page directory时
    + 通过虚拟内存地址中的L1部分完成索引
    + 接下来会走到最低级的page directory，通过虚拟内存地址中的L0部分完成索引
  + 在最低级的page directory中，得到对应于虚拟内存地址的物理内存地址
+ 举例来说，如果地址空间只使用了一个page，需要多少page table directory来映射这一个page：
  + 最理想的方案：只需要一个，但是需要2^27个PTE
  + 后者需要三个表，但是只需要3 * 512个PTE，所需的空间大大减少了

老师的补充知识点，我觉得挺有趣的：
+ 为什么PPN存储在这些page directory中，而不是一个虚拟内存地址中
  + 不能让地址翻译依赖于另一个翻译，否则可能会陷入递归的无限循环中
+ SATP呢？它存的是物理地址还是虚拟地址
  + 必须是物理地址，因为我们要用它来完成地址翻译，而不是对它进行地址翻译
+ 3级的page table是由硬件实现的
  + 所以3级 page table的查找都发生在硬件中
  + MMU是硬件的一部分而不是操作系统的一部分
  + 在XV6中，有一个函数也实现了page table的查找：
    + walk函数，在软件中实现了MMU硬件相同的功能

# 页表缓存
+ 对于一个虚拟内存地址的寻址，需要读三次内存，代价有点高
+ 所以处理器都会对于最近使用过的虚拟地址的翻译结果有缓存
  + 这个缓存被称为：Translation Lookside Buffer(TLB)

# kvminit 函数
```c
void
kvminit(){
    kernel pagetable =(pagetable t)kalloc();
    memset(kernel pagetable，0，PGSIZE);

    // uart寄存器
    kvmmaP(UART0，UARTO，PGSIZE，PTE_R | PTE_W);
    
    vmprint(kernel_pagetable);
    
    // virtio mmio磁盘接口
    kvmmaP(VIRTIo0,VIRTIo0,PGSIZE,PTE_RIPTE_W);
    
    // CLINT
    kvmmaP(CLINT，CLINT，0x10000，PTERPTE W);

    // PLIC
    kvmmaP(PLIC，PLIC，0x400000，PTERPTE W);

    // map kernel text executable and read-only.
    kvmmap(KERNBASE,KERNBASE,(uint64)etext-KERNBASE,PTE RIPTE X);

    // map kernel data and the physical RAM we'll make use of.
    kvmmap((uint64)etext,(uint64)etext,PHYSTOP-(uint64)etext, PTE R| PTE W);

    // map the trampoline for trap entry/exit to
    // the highest virtual address in the kernel（内核中最高的虚拟内存地址）
    kvmmap(TRAMPOLINE,(uint64)trampoline,PGSIZE,PTE RIPTE X);

    vmprint(kernel pagetable);
}
```

```c
//Switch h/w page table register to the kernel's page table,and enable paging
//将硬件页表寄存器切换到内核的页表，并启用分页
void
kvminithart(){
    w_satp(MAKE_SATP(kernel_pagetable));
    sfence_vma();
}
```
+ 在这条指令之前，还不存在可用的page table，所以也就不存在地址翻译
+ 执行完这条指令之后，程序计数器（Program Counter）增加了4
  + 而之后的下一条指令被执行时，程序计数器会被内存中的page table翻译
  + 之后的每一个使用的内存地址都可能对应到与之不同的物理内存地址
+ 在这条指令之前，我们使用的都是物理内存地址
+ 这条指令之后page table开始生效，所有的内存地址都变成了虚拟内存地址
