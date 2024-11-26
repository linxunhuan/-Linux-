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

## ECALL指令之后的状态
+ 先执行ecall指令
<img src=".\picture\image35.png">

+ 第一个问题，执行完了ecall之后我们现在在哪
  + 我们可以打印程序计数器（Program Counter）来查看
<img src=".\picture\image36.png">

+ 可以看到程序计数器的值变化了
  + 之前我们的程序计数器还在一个很小的地址0xde6，但是现在在一个大得多的地址
+ 我们还可以查看page table
  + 通过在QEMU中执行info mem来查看当前的page table
  + 可以看出，这还是与之前完全相同的page table，所以page table没有改变
<img src=".\picture\image37.png">

+ 根据现在的程序计数器，代码正在trampoline page的最开始，这是用户内存中一个非常大的地址
+ 所以现在我们的指令正运行在内存的trampoline page中
+ 我们可以来查看一下现在将要运行的指令
<img src=".\picture\image38.png">

+ 这些指令是内核在supervisor mode中将要执行的最开始的几条指令，也是在trap机制中最开始要执行的几条指令
+ 因为gdb有一些奇怪的行为，我们实际上已经执行了位于trampoline page最开始的一条指令（注，也就是csrrw指令）
+ 我们将要执行的是第二条指令
+ 我们可以查看寄存器
  + 对比6.3中的图可以看出，寄存器的值并没有改变
  + 这里还是用户程序拥有的一些寄存器内容
<img src=".\picture\image39.png">

+ 所以，现在寄存器里面还都是用户程序的数据，并且这些数据也还只保存在这些寄存器中
  + 所以我们需要非常小心，在将寄存器数据保存在某处之前，我们在这个时间点不能使用任何寄存器
    + 否则的话我们是没法恢复寄存器数据的
  + 如果内核在这个时间点使用了任何一个寄存器
    + 内核会覆盖寄存器内的用户数据
    + 之后如果我们尝试要恢复用户程序，我们就不能恢复寄存器中的正确数据
    + 用户程序的执行也会相应的出错
+ 我们现在在这个地址0x3ffffff000，也就是上面page table输出的最后一个page
  + 这是trampoline page
+ 我们现在正在trampoline page中执行程序，这个page包含了内核的trap处理代码
+ ecall并不会切换page table，这是ecall指令的一个非常重要的特点
+ 所以这意味着，trap处理代码必须存在于每一个user page table中
  + 因为ecall并不会切换page table，我们需要在user page table中的某个地方来执行最初的内核代码
  + 而这个trampoline page，是由内核小心的映射到每一个user page table中
    + 以使得当我们仍然在使用user page table时，内核在一个地方能够执行trap机制的最开始的一些指令
+ 这里的控制是通过STVEC寄存器完成的
  + 这是一个只能在supervisor mode下读写的特权寄存器
  + 在从内核空间进入到用户空间之前，内核会设置好STVEC寄存器指向内核希望trap代码运行的位置
<img src=".\picture\image40.png">

+ 内核已经事先设置好了STVEC寄存器的内容为0x3ffffff000
  + 这就是trampoline page的起始位置
+ STVEC寄存器的内容，就是在ecall指令执行之后，我们会在这个特定地址执行指令的原因
+ 即使trampoline page是在用户地址空间的user page table完成的映射，用户代码不能写它
  + 因为这些page对应的PTE并没有设置PTE_u标志位
  + 这也是为什么trap机制是安全的
+ 我一直在告诉你们我们现在已经在supervisor mode了
  + 但是实际上我并没有任何能直接确认当前在哪种mode下的方法
  + 不过我的确发现程序计数器现在正在trampoline page执行代码，而这些page对应的PTE并没有设置PTE_u标志位
  + 所以现在只有当代码在supervisor mode时，才可能在程序运行的同时而不崩溃
  + 所以，我从代码没有崩溃和程序计数器的值推导出,我们必然在supervisor mode
+ 我们是通过ecall走到trampoline page的，而ecall实际上只会改变三件事情：
  + 第一，ecall将代码从user mode改到supervisor mode
  + 第二，ecall将程序计数器的值保存在了SEPC寄存器
    + 我们可以通过打印程序计数器看到这里的效果
<img src=".\picture\image41.png">
尽管其他的寄存器还是还是用户寄存器，但是这里的程序计数器明显已经不是用户代码的程序计数器
这里的程序计数器是从STVEC寄存器拷贝过来的值
我们也可以打印SEPC（Supervisor Exception Program Counter）寄存器，这是ecall保存用户程序计数器的地方
<img src=".\picture\image42.png">
这个寄存器里面有熟悉的地址0xde6，这是ecall指令在用户空间的地址
所以ecall至少保存了程序计数器的数值。

  + 第三，ecall会跳转到STVEC寄存器指向的指令
+ 所以现在，ecall帮我们做了一点点工作，但是实际上我们离执行内核中的C代码还差的很远
+ 接下来：
  + 我们需要保存32个用户寄存器的内容，这样当我们想要恢复用户代码执行时，我们才能恢复这些寄存器的内容
  + 因为现在我们还在user page table，我们需要切换到kernel page table
  + 我们需要创建或者找到一个kernel stack，并将Stack Pointer寄存器的内容指向那个kernel stack
    + 这样才能给C代码提供栈 
  + 我们还需要跳转到内核中C代码的某些合理的位置
+ 当然，我们可以通过修改硬件让ecall为我们完成这些工作，而不是交给软件来完成，那么就有下面的疑问：
  + 为什么ecall不多做点工作来将代码执行从用户空间切换到内核空间呢？
  + 为什么ecall不会保存用户寄存器
    + 或者切换page table指针来指向kernel page table
    + 或者自动的设置Stack Pointer指向kernel stack
    + 或者直接跳转到kernel的C代码
  + 而不是在这里运行复杂的汇编代码？
+ 实际上，有的机器在执行系统调用时，会在硬件中完成所有这些工作
+ 但是RISC-V并不会，RISC-V秉持了这样一个观点：
  + ecall只完成尽量少必须要完成的工作，其他的工作都交给软件完成
  + 这里的原因是，RISC-V设计者想要为软件和操作系统的程序员提供最大的灵活性，这样他们就能按照他们想要的方式开发操作系统
  + 所以你可以这样想，尽管XV6并没有使用这里提供的灵活性，但是一些其他的操作系统用到了
+ 举个例子，因为这里的ecall是如此的简单，或许某些操作系统可以在不切换page table的前提下，执行部分系统调用
  + 切换page table的代价比较高
    + 如果ecall打包完成了这部分工作，那就不能对一些系统调用进行改进，使其不用在不必要的场景切换page table
  + 某些操作系统同时将user和kernel的虚拟地址映射到一个page table中，这样在user和kernel之间切换时根本就不用切换page table
    + 对于这样的操作系统来说，如果ecall切换了page table那将会是一种浪费，并且也减慢了程序的运行
  + 或许在一些系统调用过程中，一些寄存器不用保存，而哪些寄存器需要保存，哪些不需要，取决于于软件，编程语言，和编译器
    + 通过不保存所有的32个寄存器或许可以节省大量的程序运行时间，所以你不会想要ecall迫使你保存所有的寄存器
  + 最后，对于某些简单的系统调用或许根本就不需要任何stack
    + 所以对于一些非常关注性能的操作系统，ecall不会自动为你完成stack切换是极好的
+ 所以，ecall尽量的简单可以提升软件设计的灵活性

## uservec函数
+ 回到XV6和RISC-V，现在程序位于trampoline page的起始，也是uservec函数的起始
+ 我们现在需要做的第一件事情就是保存寄存器的内容
+ 在RISC-V上，如果不能使用寄存器，基本上不能做任何事情。所以，对于保存这些寄存器，我们有什么样的选择呢?
  + 在一些其他的机器中，我们或许直接就将32个寄存器中的内容写到物理内存中某些合适的位置
  + 但是我们不能在RISC-V中这样做
    + 因为在RISC-V中，**supervisor mode下的代码不允许直接访问物理内存**
    + 所以我们只能使用page table中的内容，但是从前面的输出来看，page table中也没有多少内容
  + 虽然XV6并没有使用，但是另一种可能的操作是，直接将SATP寄存器指向kernel page table
    + 之后我们就可以直接使用所有的kernel mapping来帮助我们存储用户寄存器
      + 这是合法的，因为supervisor mode可以更改SATP寄存器
      + 但是在trap代码当前的位置，也就是trap机制的最开始，我们并不知道kernel page table的地址
      + 并且更改SATP寄存器的指令，要求写入SATP寄存器的内容来自于另一个寄存器
    + 所以，为了能执行更新page table的指令，我们需要一些空闲的寄存器
      + 这样我们才能先将page table的地址存在这些寄存器中，然后再执行修改SATP寄存器的指令
+ 对于保存用户寄存器，XV6在RISC-V上的实现包括了两个部分
  + 第一个部分是，XV6在每个user page table映射了trapframe page，这样每个进程都有自己的trapframe page
    + 这个page包含了很多有趣的数据，但是现在最重要的数据是用来保存用户寄存器的32个空槽位
    + 所以，在trap处理代码中，现在的好消息是，我们在user page table有一个之前由kernel设置好的映射关系
    + 这个映射关系指向了一个可以用来存放这个进程的用户寄存器的内存位置
    + 这个位置的虚拟地址总是0x3ffffffe000
+ XV6在trapframe page中存放了什么，这部分代码在proc.h中的trapframe结构体中
<img src=".\picture\image43.png">

+ 你可以看到很多槽位的名字都对应了特定的寄存器
+ 在最开始还有5个数据，这些是内核事先存放在trapframe中的数据
+ 比如第一个数据保存了kernel page table地址，这将会是trap处理代码将要加载到SATP寄存器的数值
  + 所以，如何保存用户寄存器
    + 一半答案是，内核非常方便的将trapframe page映射到了每个user page table
    + 另一半的答案在于:我们之前提过的SSCRATCH寄存器
      + 这个由RISC-V提供的SSCRATCH寄存器，就是为接下来的目的而创建的
        + 在进入到user space之前，内核会将trapframe page的地址保存在这个寄存器中，也就是0x3fffffe000这个地址
        + 更重要的是，RISC-V有一个指令允许交换任意两个寄存器的值
          + 而SSCRATCH寄存器的作用就是保存另一个寄存器的值，并将自己的值加载给另一个寄存器
+ 如果查看trampoline.S代码，
<img src=".\picture\image44.png">

+ 第一件事情就是执行csrrw指令
  + 这个指令交换了a0和sscratch两个寄存器的内容
  + 为了看这里的实际效果，我们来打印a0
<img src=".\picture\image45.png">
a0现在的值是0x3fffffe000，这是trapframe page的虚拟地址
它之前保存在SSCRATCH寄存器中，但是我们现在交换到了a0中
我们也可以打印SSCRATCH寄存器，
<img src=".\picture\image46.png">

+ 它现在的内容是2，这是a0寄存器之前的值
  + a0寄存器保存的是write函数的第一个参数
+ 在这个场景下，是Shell传入的文件描述符2
+ 所以我们现在将a0的值保存起来了，并且我们有了指向trapframe page的指针
  + 现在我们正在朝着保存用户寄存器的道路上前进
+ 实际上，这就是trampoline.S中接下来30多个奇怪指令的工作
  + 这些指令就是的执行sd，将每个寄存器保存在trapframe的不同偏移位置
  + 因为a0在交换完之后包含的是trapframe page地址，也就是0x3fffffe000
  + 所以，每个寄存器被保存在了偏移量+a0的位置
+ 程序现在仍然在trampoline的最开始，也就是uservec函数的最开始，我们基本上还没有执行任何内容
  + 在寄存器拷贝的结束位置设置了一个断点，我们在gdb中让代码继续执行，现在我们停在了下面这条ld（load）指令
<img src=".\picture\image47.png">

+ 这条指令正在将a0指向的内存地址往后数的第8个字节开始的数据加载到Stack Pointer寄存器
  + a0的内容现在是trapframe page的地址
  + 从本节第一张图中，trapframe的格式可以看出，第8个字节开始的数据是内核的Stack Pointer（kernel_sp）
    + trapframe中的kernel_sp是由kernel在进入用户空间之前就设置好的
    + 它的值是这个进程的kernel stack
  + 所以这条指令的作用是初始化Stack Pointer指向这个进程的kernel stack的最顶端
  + 指向完这条指令之后，我们打印一下当前的Stack Pointer寄存器，
<img src=".\picture\image48.png">

+ 这是这个进程的kernel stack
  + 因为XV6在每个kernel stack下面放置一个guard page，所以kernel stack的地址都比较大
+ 下一条指令是向tp寄存器写入数据
  + 因为在RISC-V中，没有一个直接的方法来确认当前运行在多核处理器的哪个核上
    + XV6会将CPU核的编号也就是hartid保存在tp寄存器
  + 在内核中好几个地方都会使用了这个值
    + 例如，内核可以通过这个值确定某个CPU核上运行了哪些进程
+ 我们执行这条指令，并且打印tp寄存器
<img src=".\picture\image49.png">

+ 我们现在运行在CPU核0
  + 因为我之前配置了QEMU只给XV6分配一个核，所以我们只能运行在核0上
+ 下一条指令是向t0寄存器写入数据
  + 这里写入的是我们将要执行的第一个C函数的指针，也就是函数usertrap的指针
+ 我们在后面会使用这个指针
<img src=".\picture\image50.png">

+ 下一条指令是向t1寄存器写入数据
+ 这里写入的是kernel page table的地址，我们可以打印t1寄存器的内容
<img src=".\picture\image51.png">

+ 实际上严格来说，t1的内容并不是kernel page table的地址，这是你需要向SATP寄存器写入的数据
+ 它包含了kernel page table的地址，但是移位了（注，详见4.3），并且包含了各种标志位
+ 下一条指令是交换SATP和t1寄存器
  + 这条指令执行完成之后，当前程序会从user page table切换到kernel page table
  + 现在我们在QEMU中打印page table，可以看出与之前的page table完全不一样
<img src=".\picture\image52.png">

+ 现在这里输出的是由内核设置好的巨大的kernel page table
+ 所以现在我们成功的切换了page table
  + Stack Pointer指向了kernel stack
+ 我们有了kernel page table，可以读取kernel data
+ 我们已经准备好了执行内核中的C代码了
+ 这里还有个问题，为什么代码没有崩溃？
  + 毕竟我们在内存中的某个位置执行代码，程序计数器保存的是虚拟地址
  + 如果我们切换了page table，为什么同一个虚拟地址不会通过新的page table寻址走到一些无关的page中？
    + 原因：因为我们还在trampoline代码中，而trampoline代码在用户空间和内核空间都映射到了同一个地址
    + trampoline page在user page table中的映射与kernel page table中的映射是完全一样的
    + 这两个page table中其他所有的映射都是不同的，只有trampoline page的映射是一样的
      + 因此我们在切换page table时，寻址的结果不会改变，我们实际上就可以继续在同一个代码序列中执行程序而不崩溃
    + 这是trampoline page的特殊之处，它同时在user page table和kernel page table都有相同的映射关系
+ 之所以叫trampoline page，是因为你某种程度在它上面“弹跳”了一下，然后从用户空间走到了内核空间
+ 最后一条指令是_jr t0_
  + 执行了这条指令，我们就要从trampoline跳到内核的C代码中
  + 这条指令的作用是跳转到t0指向的函数中
  + 我们打印t0对应的一些指令，
<img src=".\picture\image53.png">

+ 可以看到t0的位置对应于一个叫做usertrap函数的开始
+ 接下来我们就要以kernel stack，kernel page table跳转到usertrap函数

## usertrap函数
+ usertrap函数是位于trap.c文件的一个函数
<img src=".\picture\image54.png">

+ 既然我们已经运行在C代码中，接下来，在gdb中输入tui enable打开对于C代码的展示
<img src=".\picture\image55.png">

+ 我们现在在一个更加正常的世界中，我们正在运行C代码，应该会更容易理解
+ 我们仍然会读写一些有趣的控制寄存器，但是环境比起汇编语言来说会少了很多晦涩
+ 有很多原因都可以让程序运行进入到usertrap函数中来
  + 比如系统调用
  + 运算时除以0
  + 使用了一个未被映射的虚拟地址
  + 或者是设备中断
+ usertrap某种程度上存储并恢复硬件状态
    + 但是它也需要检查触发trap的原因
    + 以确定相应的处理方式
  + 我们在接下来执行usertrap的过程中会同时看到这两个行为
+ 接下来，让我们一步步执行usertrap函数
<img src=".\picture\image56.png">

+ 它做的第一件事情是更改STVEC寄存器
+ 取决于trap是来自于用户空间还是内核空间，实际上XV6处理trap的方法是不一样的
+ 目前为止，我们只讨论过当trap是由用户空间发起时会发生什么
+ 如果trap从内核空间发起，将会是一个非常不同的处理流程
  + 因为从内核发起的话，程序已经在使用kernel page table
  + 所以当trap发生时，程序执行仍然在内核的话，很多处理都不必存在
+ 在内核中执行任何操作之前
  + usertrap中先将STVEC指向了kernelvec变量
  + 这是内核空间trap处理代码的位置，而不是用户空间trap处理代码的位置
<img src=".\picture\image57.png">

+ 我们需要知道当前运行的是什么进程
  + 我们通过调用myproc函数来做到这一点
+ myproc函数实际上会查找一个根据当前CPU核的编号索引的数组
  + CPU核的编号是hartid
    + 如果你还记得，我们之前在uservec函数中将它存在了tp寄存器
    + 这是myproc函数找出当前运行进程的方法
<img src=".\picture\image58.png">

+ 接下来我们要保存用户程序计数器，它仍然保存在SEPC寄存器中，但是可能发生这种情况：
  + 当程序还在内核中执行时，我们可能切换到另一个进程，并进入到那个程序的用户空间
  + 然后那个进程可能再调用一个系统调用
    + 进而导致SEPC寄存器的内容被覆盖
  + 所以，我们需要保存当前进程的SEPC寄存器到一个与该进程关联的内存中
    + 这样这个数据才不会被覆盖
+ 这里我们使用trapframe来保存这个程序计数器
<img src=".\picture\image59.png">

+ 接下来我们需要找出我们现在会在usertrap函数的原因
  + 根据触发trap的原因，RISC-V的SCAUSE寄存器会有不同的数字
  + 数字8表明，我们现在在trap代码中是因为系统调用
  + 可以打印SCAUSE寄存器，它的确包含了数字8，我们的确是因为系统调用才走到这里的
<img src=".\picture\image60.png">

+ 所以，我们可以进到这个if语句中
+ 接下来第一件事情是检查是不是有其他的进程杀掉了当前进程
+ 但是我们的Shell没有被杀掉，所以检查通过
<img src=".\picture\image61.png">

+ 在RISC-V中，存储在SEPC寄存器中的程序计数器，是用户程序中触发trap的指令的地址
+ 但是当我们恢复用户程序时，我们希望在下一条指令恢复
  + 也就是ecall之后的一条指令
+ 所以对于系统调用，我们对于保存的用户程序计数器加4
  + 这样我们会在ecall的下一条指令恢复，而不是重新执行ecall指令
<img src=".\picture\image62.png">

+ XV6会在处理系统调用的时候使能中断
  + 这样中断可以更快的服务
  + 有些系统调用需要许多时间处理
+ 中断总是会被RISC-V的trap硬件关闭
  + 所以在这个时间点，我们需要显式的打开中断
<img src=".\picture\image63.png">

+ 下一行代码中，我们会调用syscall函数。这个函数定义在syscall.c

<img src=".\picture\image64.png">

+ 它的作用是从syscall表单中，根据系统调用的编号查找相应的系统调用函数
+ Shell调用的write函数将a7设置成了系统调用编号,对于write来说就是16
  + 所以syscall函数的工作就是获取由trampoline代码保存在trapframe中a7的数字
  + 然后用这个数字索引实现了每个系统调用的表单
+ 我们可以打印num，的确是16。这与Shell调用的write函数写入的数字是一致的
<img src=".\picture\image65.png">

+ 之后查看通过num索引得到的函数，正是sys_write函数
  + sys_write函数是内核对于write系统调用的具体实现
+ 这里有件有趣的事情，系统调用需要找到它们的参数
  + write函数的参数:
    + 文件描述符2，写入数据缓存的指针，写入数据的长度2
+ syscall函数直接通过trapframe来获取这些参数
  + 就像这里刚刚可以查看trapframe中的a7寄存器一样
  + 我们可以查看a0寄存器，这是第一个参数，a1是第二个参数，a2是第三个参数
<img src=".\picture\image66.png">
现在syscall执行了真正的系统调用，之后sys_write返回了
<img src=".\picture\image67.png">

+ 这里向trapframe中的a0赋值的原因是：
  + 所有的系统调用都有一个返回值
    + 比如write会返回实际写入的字节数
    + 而RISC-V上的C代码的习惯是函数的返回值存储于寄存器a0
  + 所以为了模拟函数的返回，我们将返回值存储在trapframe的a0中
  + 之后，当我们返回到用户空间
    + trapframe中的a0槽位的数值会写到实际的a0寄存器
    + Shell会认为a0寄存器中的数值是write系统调用的返回值
  + 执行完这一行代码之后，我们打印这里trapframe中a0的值，可以看到输出2
<img src=".\picture\image68.png">

+ 这意味这sys_write的返回值是2，符合传入的参数，这里只写入了2个字节
+ 从syscall函数返回之后，我们回到了trap.c中的usertrap函数
<img src=".\picture\image69.png">

+ 我们再次检查当前用户进程是否被杀掉了，因为我们不想恢复一个被杀掉的进程
+ 当然，在我们的场景中，Shell没有被杀掉
<img src=".\picture\image70.png">

+ 最后，usertrap调用了一个函数usertrapret

## usertrapret函数
<img src=".\picture\image71.png">

+ usertrap函数的最后调用了usertrapret函数，来设置好之前说过的
  + 在返回到用户空间之前内核要做的工作
+ 我们可以查看这个函数的内容
<img src=".\picture\image72.png">

+ 它首先关闭了中断
  + 我们之前在系统调用的过程中是打开了中断的
    + 这里关闭中断是因为:
      + 我们将要更新STVEC寄存器来指向用户空间的trap处理代码
    + 而之前在内核中的时候，我们指向的是内核空间的trap处理代码（6.6）
    + 我们关闭中断因为:
      + 当我们将STVEC更新到指向用户空间的trap处理代码时，我们仍然在内核中执行代码
      + 如果这时发生了一个中断，那么程序执行会走向用户空间的trap处理代码
      + 即便我们现在仍然在内核中，出于各种各样具体细节的原因，这会导致内核出错。所以我们这里关闭中断
<img src=".\picture\image73.png">

+ 在下一行我们设置了STVEC寄存器指向trampoline代码
  + 在那里最终会执行sret指令返回到用户空间
+ 位于trampoline代码最后的sret指令会重新打开中断
+ 这样，即使我们刚刚关闭了中断，当我们在执行用户代码时中断是打开的
<img src=".\picture\image74.png">

+ 接下来的几行填入了trapframe的内容，这里的代码就是：
  + 存储了kernel page table的指针
  + 存储了当前用户进程的kernel stack
  + 存储了usertrap函数的指针，这样trampoline代码才能跳转到这个函数（注，详见6.5中 ld t0 (16)a0 指令）
  + 从tp寄存器中读取当前的CPU核编号，并存储在trapframe中，这样trampoline代码才能恢复这个数字，因为用户代码可能会修改这个数字
+ 现在我们在usertrapret函数中，我们正在设置trapframe中的数据
+ 这样下一次从用户空间转换到内核空间时可以用到这些数据
```
用户寄存器（User Registers）必须在汇编代码中保存
因为任何需要经过编译器的语言，例如C语言，都不能修改任何用户寄存器
所以对于用户寄存器，必须要在进入C代码之前在汇编代码中保存好
```
+ 接下来我们要设置SSTATUS寄存器，这是一个控制寄存器
+ 这个寄存器的SPP bit位控制了sret指令的行为
  + 该bit为0表示下次执行sret的时候，我们想要返回user mode而不是supervisor mode
  + 这个寄存器的SPIE bit位控制了，在执行完sret之后，是否打开中断
  + 因为我们在返回到用户空间之后，我们的确希望打开中断
    + 所以这里将SPIE bit位设置为1
  + 修改完这些bit位之后，我们会把新的值写回到SSTATUS寄存器
<img src=".\picture\image75.png">

+ 我们在trampoline代码的最后执行了sret指令
+ 这条指令会将程序计数器设置成SEPC寄存器的值，所以现在我们将SEPC寄存器的值设置成之前保存的用户程序计数器的值
+ 在不久之前，我们在usertrap函数中将用户程序计数器保存在trapframe中的epc字段
<img src=".\picture\image76.png">

+ 接下来，我们根据user page table地址生成相应的SATP值，这样我们在返回到用户空间的时候才能完成page table的切换
  + 实际上，我们会在汇编代码trampoline中完成page table的切换
    + 并且也只能在trampoline中完成切换
  + 因为只有trampoline中代码是同时在用户和内核空间中映射
  + 但是我们现在还没有在trampoline代码中，我们现在还在一个普通的C函数中
    + 所以这里我们将page table指针准备好
    + 并将这个指针作为第二个参数传递给汇编代码
    + 这个参数会出现在a1寄存器
+ 倒数第二行的作用是计算出我们将要跳转到汇编代码的地址
  + 我们期望跳转的地址是tampoline中的userret函数
    + 这个函数包含了所有能将我们带回到用户空间的指令
  + 所以这里我们计算出了userret函数的地址
    + 倒数第一行，将fn指针作为一个函数指针
    + 执行相应的函数（也就是userret函数）并传入两个参数
    + 两个参数存储在a0，a1寄存器中

## userret函数
+ 现在程序执行又到了trampoline代码
<img src=".\picture\image77.png">

+ 第一步是切换page table
+ 在执行_csrw satp, a1_之前，page table应该还是巨大的kernel page table
+ 这条指令会将user page table（在usertrapret中作为第二个参数传递给了这里的userret函数，所以存在a1寄存器中）存储在SATP寄存器中
+ 执行完这条指令之后，page table就变成了小得多的user page table
  + 但是幸运的是，user page table也映射了trampoline page，所以程序还能继续执行而不是崩溃
  + (注，sfence.vma是清空页表缓存)
<img src=".\picture\image78.png">

+ 在uservec函数中，第一件事情就是交换SSRATCH和a0寄存器
  + 而这里，我们将SSCRATCH寄存器恢复成保存好的用户的a0寄存器
  + 在这里a0是trapframe的地址
    + 因为C代码usertrapret函数中将trapframe地址作为第一个参数传递过来了
  + 112是a0寄存器在trapframe中的位置
+ （注，这里有点绕，本质就是通过当前的a0寄存器找出存在trapframe中的a0寄存器）
+ 我们先将这个地址里的数值保存在t0寄存器中，之后再将t0寄存器的数值保存在SSCRATCH寄存器中
+ 为止目前，所有的寄存器内容还是属于内核
<img src=".\picture\image79.png">

+ 接下来的这些指令将a0寄存器指向的trapframe中
  + 之前保存的寄存器的值加载到对应的各个寄存器中
  + 之后，我们离能真正运行用户代码就很近了
+ 现在我们打印所有的寄存器，
<img src=".\picture\image80.png">

+ 我不确定你们是否还记得，但是这些寄存器的值就是我们在最最开始看到的用户寄存器的值
  + 例如SP寄存器保存的是user stack地址
    + 这是一个在较小的内存地址
  + a1寄存器是我们传递给write的buffer指针
  + a2是我们传递给write函数的写入字节数
  + a0寄存器现在还是个例外，它现在仍然是指向trapframe的指针，而不是保存了的用户数据
<img src=".\picture\image81.png">

+ 接下来，在我们即将返回到用户空间之前，我们交换SSCRATCH寄存器和a0寄存器的值
  + 前面我们看过了SSCRATCH现在的值是系统调用的返回值2
    + a0寄存器是trapframe的地址
  + 交换完成之后，a0持有的是系统调用的返回值，SSCRATCH持有的是trapframe的地址
  + 之后trapframe的地址会一直保存在SSCRATCH中，直到用户程序执行了另一次trap
+ 现在我们还在kernel中
+ sret是我们在kernel中的最后一条指令，当我执行完这条指令：
  + 程序会切换回user mode
  + SEPC寄存器的数值会被拷贝到PC寄存器（程序计数器）
  + 重新打开中断
+ 现在我们回到了用户空间。打印PC寄存器，
<img src=".\picture\image82.png">

+ 这是一个较小的指令地址，非常像是在用户内存中
+ 如果我们查看sh.asm，可以看到这个地址是write函数的ret指令地址
<img src=".\picture\image83.png">

+ 所以，现在我们回到了用户空间
+ 执行完ret指令之后我们就可以从write系统调用返回到Shell中了
+ 或者更严格的说，是从触发了系统调用的write库函数中返回到Shell中
------------------------------------
+ 最后总结一下，系统调用被刻意设计的看起来像是函数调用，但是背后的user/kernel转换比函数调用要复杂的多
+ 之所以这么复杂，很大一部分原因是要保持user/kernel之间的隔离性，内核不能信任来自用户空间的任何内容
+ 另一方面，XV6实现trap的方式比较特殊，XV6并不关心性能
+ 但是通常来说，操作系统的设计人员和CPU设计人员非常关心如何提升trap的效率和速度
+ 必然还有跟我们这里不一样的方式来实现trap，当你在实现的时候，可以从以下几个问题出发：
  + 硬件和软件需要协同工作，你可能需要重新设计XV6，重新设计RISC-V来使得这里的处理流程更加简单，更加快速
  + 另一个需要时刻记住的问题是，恶意软件是否能滥用这里的机制来打破隔离性