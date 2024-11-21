# Trap机制

+ 程序运行是完成用户空间和内核空间的切换在以下状态均会发生：

  + 程序执行系统调用
  + 程序出现了类似page fault、运算时除以0的错误
  + 一个设备触发了中断使得当前程序运行需要响应内核设备驱动
+ 这里用户空间和内核空间的切换通常被称为trap，

  + trap涉及了许多小心的设计和重要的细节，这些细节对于实现安全隔离和性能来说非常重要
  + trap机制要尽可能的简单
+ 我们有一些用户应用程序，例如Shell

  + 运行在用户空间，同时我们还有内核空间
  + Shell可能会执行系统调用，将程序运行切换到内核
    + 比如XV6启动之后Shell输出的一些提示信息，就是通过执行write系统调用来输出的
+ 我们需要清楚如何让程序的运行：

  + 从只拥有user权限并且位于用户空间的Shell，切换到拥有supervisor权限的内核
  + 在这个过程中，硬件的状态将会非常重要
    + 因为我们很多的工作都是将硬件从适合运行用户应用程序的状态，改变到适合运行内核代码的状态
    + 我们最关心的状态可能是32个用户寄存器
      + RISC-V总共有32个比如a0，a1这样的寄存器
      + 用户应用程序可以使用全部的寄存器，并且使用寄存器的指令性能是最好的
+ 在硬件中还有一个寄存器叫做程序计数器（Program Counter Register）

  + 表明当前mode的标志位，这个标志位表明了当前是supervisor mode还是user mode
  + 当我们在运行Shell的时候，自然是在user mode。
+ 还有一堆控制CPU工作方式的寄存器

  + 比如SATP（Supervisor Address Translation and Protection）寄存器，它包含了指向page table的物理内存地址
+ 还有一些对于今天讨论非常重要的寄存器
  + 比如STVEC（Supervisor Trap Vector Base Address Register）寄存器，它指向了内核中处理trap的指令的起始地址
  + SEPC（Supervisor Exception Program Counter）寄存器，在trap的过程中保存程序计数器的值
  + SSRATCH（Supervisor Scratch Register）寄存器
+ 这些寄存器表明了执行系统调用时计算机的状态
-------------------
+ 在trap的最开始，CPU的所有状态都设置成运行用户代码而不是内核代码
+ 在trap处理的过程中，我们实际上需要更改一些这里的状态，或者对状态做一些操作
  + 这样我们才可以运行系统内核中普通的C程序
+ 接下来我们先来预览一下需要做的操作：
  + 首先，我们需要保存32个用户寄存器
    + 因为很显然我们需要恢复用户应用程序的执行，尤其是当用户程序随机的被设备中断所打断时
      + 我们希望内核能够响应中断，之后在用户程序完全无感知的情况下再恢复用户代码的执行
      + 所以这意味着32个用户寄存器不能被内核弄乱
      + 但是这些寄存器又要被内核代码所使用，所以在trap之前，你必须先在某处保存这32个用户寄存器
+ 程序计数器也需要在某个地方保存，它几乎跟一个用户寄存器的地位是一样的，我们需要能够在用户程序运行中断的位置继续执行用户程序
+ 我们需要将mode改成supervisor mode，因为我们想要使用内核中的各种各样的特权指令。
+ SATP寄存器现在正指向user page table
  + user page table只包含了用户程序所需要的内存映射和一两个其他的映射
  + 并没有包含整个内核数据的内存映射
  + 所以在运行内核代码之前，我们需要将SATP指向kernel page table
+ 我们需要将堆栈寄存器指向位于内核的一个地址
  + 因为我们需要一个堆栈来调用内核的C函数
+ 操作系统的一些high-level的目标能帮我们过滤一些实现选项
  + 其中一个目标是安全和隔离，我们不想让用户代码介入到这里的user/kernel切换
    + 否则有可能会破坏安全性
  + 所以这意味着，**trap中涉及到的硬件和内核机制不能依赖任何来自用户空间东西**
    + 比如说我们不能依赖32个用户寄存器，它们可能保存的是恶意的数据
    + 所以，XV6的trap机制不会查看这些寄存器，而只是将它们保存起来
## 寄存器supervisor mode介绍
+ 可以读写控制寄存器了
  + 比如说，当在supervisor mode时，可以：读写SATP寄存器
    + 也就是page table的指针
    + STVEC，也就是处理trap的内核指令地址
    + SEPC，保存当发生trap时的程序计数器
    + SSCRATCH等等
  + 在supervisor mode你可以读写这些寄存器，而用户代码不能做这样的操作
  + 另一件事情supervisor mode可以做的是
    + 使用PTE_U标志位为0的PTE
      + 当PTE_U标志位为1的时候，表明用户代码可以使用这个页表
      + 如果为0，则只有supervisor mode可以使用这个页表
+ supervisor mode中的代码并不能读写任意物理地址
  + 在supervisor mode中，就像普通的用户代码一样，也需要通过page table来访问内存
  + 如果一个虚拟地址并不在当前由SATP指向的page table中，又或者SATP指向的page table中PTE_U=1，那么supervisor mode不能使用那个地址
  + 即使我们在supervisor mode，我们还是受限于当前page table设置的虚拟地址
# 进入到内核空间时，trap代码的执行流程
## 在Shell中调用write系统调用的流程
+ 第一步：write通过执行ECALL指令来执行系统调用
  + ECALL指令会切换到具有supervisor mode的内核中
  + 在这个过程中，内核中执行的第一个指令是一个由汇编语言写的函数，叫做uservec
  + 这个函数是内核代码trampoline.s文件的一部分
+ 第二步：代码执行跳转到了由C语言实现的函数usertrap中
  + 这个函数在trap.c中
+ 第三步：在usertrap这个C函数中，我们执行了一个叫做syscall的函数
  + 这个函数会在一个表单中，根据传入的代表系统调用的数字进行查找，并在内核中执行具体实现了系统调用功能的函数
  + 对于我们来说，这个函数就是sys_write
+ 第四步：调用sys_write，将要显示数据输出到console上
  + 当它完成了之后，它会返回给syscall函数
+ 第五步：调用一个函数叫做usertrapret
  + 它也位于trap.c中，在syscall函数内
  + 现在相当于在ECALL之后中断了用户代码的执行，这个函数为了返回到用户空间
+ 第六步：最终还有一些工作只能在汇编语言中完成
  + 存在于trampoline.s文件中的userret函数中
  + 在这个汇编函数中会调用机器指令返回到用户空间，并且恢复ECALL之后的用户程序的执行

## ECALL指令之前的状态
+ Shell调用write时，实际上调用的是关联到Shell的一个库函数
  + 这个库函数的源代码，在usys.s
<img src=".\picture\image26.png">
+ 首先将SYS_write加载到a7寄存器，SYS_write是常量16
  + 这里告诉内核，我想要运行第16个系统调用，而这个系统调用正好是write
+ 之后这个函数中执行了ecall指令，从这里开始代码执行跳转到了内核
  + 内核完成它的工作之后，代码执行会返回到用户空间，继续执行ecall之后的指令，也就是ret，最终返回到Shell中
  + 所以ret从write库函数返回到了Shell中
+ 现在在ecall指令处放置一个断点
  + 为了能放置断点，我们需要知道ecall指令的地址
    + 我们可以通过查看由XV6编译过程产生的sh.asm找出这个地址
    + sh.asm是带有指令地址的汇编代码（注，asm文件3.7有介绍）
  + 现在在ecall指令处放置一个断点，这条指令的地址是0xde6。
<img src=".\picture\image27.png">

+ XV6在Shell代码中正好在执行ecall之前就会停住
<img src=".\picture\image28.png">

+ 从gdb可以看出，我们下一条要执行的指令就是ecall
+ 下一步，打印程序计数器（Program Counter），正好在我们期望的位置0xde6
<img src=".\picture\image29.png">

+ 输入_info reg_打印全部32个用户寄存器
<img src=".\picture\image30.png">

+ a0是文件描述符2
+ a1是Shell想要写入字符串的指针
+ a2是想要写入的字符数
+ 我们还可以通过打印Shell想要写入的字符串内容，来证明断点停在我们认为它应该停在的位置
<img src=".\picture\image31.png">

+ 可以看出，输出的确是（$）和一个空格。所以，现在位于我们期望所在的write系统调用函数中
+ 有一件事情需要注意，上图的寄存器中，程序计数器（pc）和堆栈指针（sp）的地址现在都在距离0比较近的地址
  + 这进一步印证了当前代码运行在用户空间，因为用户空间中所有的地址都比较小
  + 但是一旦我们进入到了内核，内核会使用大得多的内存地址

+ 系统调用的时间点会有大量状态的变更，其中一个最重要的需要变更的状态，并且在它变更之前我们对它还有依赖的，就是是当前的page table
+ 我们可以查看STAP寄存器
<img src=".\picture\image32.png">

+ 这里输出的是物理内存地址，它并没有告诉我们有关page table中的映射关系是什么，page table长什么样
+ 但是幸运的是，在QEMU中有一个方法可以打印当前的page table
  + 从QEMU界面，输入_ctrl a + c_可以进入到QEMU的console
  + 之后输入_info mem_，QEMU会打印完整的page table
<img src=".\picture\image33.png">

+ 这是个非常小的page table，它只包含了6条映射关系
+ 这是用户程序Shell的page table
  + Shell是一个非常小的程序
  + 这6条映射关系是有关Shell的指令和数据
  + 以及一个无效的page用来作为guard page
    + 防止Shell尝试使用过多的stack page
  + 我们可以看出这个page是无效的
    + 因为在attr这一列它并没有设置u标志位（第三行）
      + attr这一列是PTE的标志位
      + 第三行的标志位是rwx表明这个page可以读，可以写，也可以执行指令
      + 之后的是u标志位，它表明PTE_u标志位是否被设置
      + 用户代码只能访问u标志位设置了的PTE

+ 现在，我们有了这个小小的page table
  + 最后两条PTE的虚拟地址非常大，非常接近虚拟地址的顶端
  + 这两个page分别是trapframe page和trampoline page
  + 它们都没有设置u标志，所以用户代码不能访问这两条PTE
  + 一旦我们进入到了supervisor mode，我们就可以访问这两条PTE了
+ 对于这里page table，有一件事情需要注意：
  + 它并没有包含任何内核部分的地址映射
  + 这里既没有对于kernel data的映射
  + 也没有对于kernel指令的映射
+ 除了最后两条PTE，这个page table几乎是完全为用户代码执行而创建，所以它对于在内核执行代码并没有直接特殊的作用

+ 接下来，在Shell中打印出write函数的内容
<img src=".\picture\image34.png">

+ 程序计数器现在指向ecall指令，我们接下来要执行ecall指令
+ 现在还在用户空间，但是马上就要进入内核空间了

















