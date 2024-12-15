# 虚拟机
## 为什么要使用虚拟机
+ 在架构的最底层，位于硬件之上存在一个Virtual Machine Monitor（VMM）
  + 它取代了标准的操作系统内核
+ VMM的工作是模拟多个计算机用来运行Guest操作系统
+ VMM往上一层，如果对比一个操作系统的架构应该是用户空间，但是现在是叫做Guest空间
+ 所以在今天的架构图里面，上面是Guest空间，下面是Host空间
  + 也就是上面运行Guest操作系统，下面运行VMM
<img src=".\picture\image177.png">

+ 在Guest空间，会有一个或者多个Guest操作系统内核，或许其中一个是Linux kernel
+ 这里的Linux kernel会觉得自己就是个普通的内核，并在自己之上还运行一堆用户进程
  + 例如VI，C Compiler
+ 所以，在Host空间运行的是VMM，在Guest空间运行的是普通的操作系统
  + 除此之外，在Guest空间又可以分为Guest Supervisor Mode
    + 也就是Guest操作系统内核运行的模式
  + 和Guest User Mode
<img src=".\picture\image178.png">

+ 在操作系统的架构中，内核之上提供的封装单元是我们熟悉的进程，内核管理的是多个用户进程
+ 而在VMM的架构中，VMM之上提供的封装单元是对计算机的模拟
+ VMM的架构使得我们可以从另一个角度重新审视我们讨论过的内容
  + 例如内存分配，线程调度等等
  + 这或许可以给我们一些新的思路并带回到传统的操作系统内核中
  + 所以，在虚拟机场景下，大部分的开发设计研究工作，从传统的内核移到了VMM
    + 某种程度上来说，传统操作系统内核的内容下移了一层到了VMM
---------------------------
+ 今天课程的第一部分我将会讨论如何实现我们自己的虚拟机
+ 这里假设我们要模拟的是RISC-V，并运行针对RISC-V设计的操作系统
  + 例如XV6
  + 我们的目的是让运行在Guest中的代码完全不能区分自己是运行在一个虚拟机还是物理机中
    + 因为我们希望能在虚拟机中运行任何操作系统，甚至是你没有听说过的操作系统
    + 这意味着对于任何操作系统的行为包括使用硬件的方式，虚拟机都必须提供提供对于硬件的完全相同的模拟
    + 这样任何在真实硬件上能工作的代码，也同样能在虚拟机中工作
+ 除了不希望Guest能够发现自己是否运行在虚拟机中，我们也不希望Guest可以从虚拟机中逃逸
  + 很多时候人们使用虚拟机是因为**它为不被信任的软件甚至对于不被信任的操作系统提供了严格的隔离**
  + 假设你是Amazon，并且你出售云服务
    + 通常是你的客户提供了运行在虚拟机内的操作系统和应用程序
    + 所以有可能你的客户运行的不是普通的Linux而是一个特殊的修改过的Linux
    + 并且会试图突破虚拟机的限制来访问其他用户的虚拟机或者访问Amazon用来实现虚拟机隔离的VMM
    + 所以Guest不能从虚拟机中逃逸还挺重要的
      + Guest可以通过VMM使用内存，但是不能使用不属于自己的内存
      + 类似的，Guest也不应该在没有权限的时候访问存储设备或者网卡
    + 虚拟机在很多方面比普通的Linux进程提供了更加严格的隔离
      + Linux进程经常可以相互交互，它们可以杀掉别的进程，它们可以读写相同的文件，或者通过pipe进行通信
      + 但是在一个普通的虚拟机中，所有这些都不被允许
      + 运行在同一个计算机上的不同虚拟机，彼此之间是通过VMM完全隔离的
      + 所以出于安全性考虑人们喜欢使用虚拟机，这是一种可以运行未被信任软件的方式，同时又不用担心bug和恶意攻击
## Trap-and-Emulate --- Trap
我们该如何构建我们自己的VMM呢？
+ 一种实现方式是完全通过软件来实现
  + 一个类似QEMU的软件
    + 这个软件读取包含了XV6内核指令的文件
    + 查看每一条指令并模拟RISC-V的状态
    + 这里的状态包括了通过软件模拟32个寄存器
  + 软件读取每条指令，确定指令类型，再将指令应用到通过软件模拟的32个寄存器和控制寄存器中
  + 实际中有的方案就是这么做的
    + 虽然说考虑到细节还需要做很多工作，但是这种方案从概念上来说很简单直观
+ 但是纯软件解析的虚拟机方案应用的并不广泛
  + 因为它们很慢
  + 如果你按照这种方式实现虚拟机，那么Guest应用程序的运行速度将远低于运行在硬件上
  + 因为你的VMM在解析每一条Guest指令的时候，都可能要转换成几十条实际的机器指令
  + 所以这个方案中的Guest的运行速度比一个真实的计算机要慢几个数量级
  + 在云计算中，这种实现方式非常不实用。所以人们并不会通过软件解析来在生产环境中构建虚拟机
+ 相应的，一种广泛使用的策略是在真实的CPU上运行Guest指令
  + 所以如果我们要在VMM之上运行XV6，我们需要先将XV6的指令加载到内存中
  + 之后再跳转到XV6的第一条指令，这样你的计算机硬件就能直接运行XV6的指令
  + 当然，这要求你的计算机拥有XV6期望的处理器（注，也就是RISC-V）
+ 但是实际中你又不能直接这么做
  + 因为当你的Guest操作系统执行了一个privileged指令之后，就会出现问题
    + （也就是在普通操作系统中只能在kernel mode中执行的指令）
  + 现在我们在虚拟机里面运行了操作系统内核，而内核会执行需要privileged权限指令
    + 比如说加载一个新的Page Table到RISC-V的SATP寄存器中
    + 而这时就会出现问题
+ 我们将Guest kernel按照一个Linux中的普通用户进程来运行
  + 所以Guest kernel现在运行在User mode
  + 而在User mode加载SATP寄存器是个非法的操作，这会导致我们的程序（也就是虚拟机）crash
  + 但是如果我们将Guest kernel运行在宿主机的Supervisor mode（也就是kernel mode）
    + 那么我们的Guest kernel不仅能够修改真实的Page Table，同时也可以从虚拟机中逃逸
    + 因为它现在可以控制PTE（Page Table Entry）的内容，并且读写任意的内存内容
    + 所以我们不能直接简单的在真实的CPU上运行Guest kernel
-----------------------------------
+ 相应的，这里会使用一些技巧
  + 首先将Guest kernel运行在宿主机的User mode
    + 这意味着，当我们自己写了一个VMM，然后通过VMM启动了一个XV6系统
    + VMM会将XV6的kernel指令加载到内存的某处，再设置好合适的Page Table使得XV6看起来自己的内存是从地址0开始向高地址走
    + 之后VMM会使用trap或者sret指令来跳转到位于User mode的Guest操作系统的第一条指令
      + 这样不论拥有多少条指令，Guest操作系统就可以一直执行下去
    + 一旦Guest操作系统需要使用privileged指令
      + 因为它当前运行在User mode而不是Supervisor mode，会使得它触发trap并走回到我们的VMM中
      + （在一个正常操作系统中，如果在User mode执行privileged指令，会通过trap走到内核，但是现在VMM替代了内核）
        + 之后我们就可以获得控制权
    + 所以当Guest操作系统尝试修改SATP寄存器，RISC-V处理器会通过trap走回到我们的VMM中，之后我们的VMM就可以获得控制权
      + 并且我们的VMM也可以查看是什么指令引起的trap，并做适当的处理
      + 这里核心的点在于Guest操作系统并没有实际的设置SATP寄存器
## Trap-and-Emulate --- Emulate
+ VMM会为每一个Guest维护一套虚拟状态信息
  + 所以VMM里面会维护虚拟的STVEC寄存器，虚拟的SEPC寄存器以及其他所有的privileged寄存器
  + 当Guest操作系统运行指令需要读取某个privileged寄存器时
    + 首先会通过trap走到VMM
      + 因为在用户空间读取privileged寄存器是非法的
    + 之后VMM会检查这条指令并发现这是一个比如说读取SEPC寄存器的指令
    + 之后VMM会模拟这条指令，并将自己维护的虚拟SEPC寄存器，拷贝到trapframe的用户寄存器中
    + 之后，VMM会将trapframe中保存的用户寄存器拷贝回真正的用户寄存器，通过sret指令，使得Guest从trap中返回
      + 这时，用户寄存器a0里面保存的就是SEPC寄存器的值了，之后Guest操作系统会继续执行指令
      + 最终，Guest读到了VMM替自己保管的虚拟SEPC寄存器
+ 在这种虚拟机的实现中，Guest整个运行在用户空间
  + 任何时候它想要执行需要privilege权限的指令时，会通过trap走到VMM，VMM可以模拟这些指令
    + 这种实现风格叫做Trap and Emulate
    + 可以完全通过软件实现这种VMM
      + 也就是说可以只通过修改软件就将XV6变成一个可以运行在RISC-V上的VMM，然后再在之上运行XV6虚拟机
      + 当然，与常规的XV6一样，VMM需要运行在Supervisor mode
+ 所有以S开头的寄存器，也就是所有的Supervisor控制寄存器都必须保存在虚拟状态信息中
  + 同时还有一些信息并不能直接通过这些控制寄存器体现，但是又必须保存在这个虚拟状态信息中
  + 其中一个信息就是mode
    + VMM需要知道虚拟机是运行在Guest user mode还是Guest Supervisor mode
    + 例如，Guest中的用户代码尝试执行privileged指令，比如读取SCAUSE寄存器，这也会导致trap并走到VMM
    + 但是这种情况下VMM不应该模拟指令并返回
      + 因为这并不是一个User mode中的合法指令
    + 所以VMM需要跟踪Guest当前是运行在User mode还是Supervisor mode，所以在虚拟状态信息里面也会保存mode
-------------------------------------------------
VMM怎么知道Guest当前的mode呢？
+ 当Guest从Supervisor mode返回到User mode时会执行sret指令，而sret指令又是一个privileged指令
  + 所以会通过trap走到VMM，进而VMM可以看到Guest正在执行sret指令，并将自己维护的mode从Supervisor变到User
  + 虚拟状态信息中保存的另外一个信息是hartid
    + 它代表了CPU核的编号
    + 即使通过privileged指令，也不能直接获取这个信息
      + VMM需要跟踪当前模拟的是哪个CPU
  + 实际中，在不同类型的CPU上实现Trap and Emulate虚拟机会有不同的难度
    + 不过RISC-V特别适合实现Trap and Emulate虚拟机，因为RISC-V的设计人员在设计指令集的时候就考虑了Trap and Emulate虚拟机的需求
    + 举个例子，设计人员确保了每个在Supervisor mode下才能执行的privileged指令
      + 如果在User mode执行都会触发trap
      + 可以通过这种机制来确保VMM针对Guest中的每个privileged指令，都能看到一个trap
+ 所以，当Guest执行sret指令从Supervisor mode进入到User mode
  + 因为sret是privileged指令，会通过trap进入到VMM
  + VMM会更新虚拟状态信息中的mode为User mode
    + 尽管当前的真实mode还是Supervisor mode
    + 因为我们还在执行VMM中的代码
  + 在VMM从trap中返回之前，VMM会将真实的SEPC寄存器设置成自己保存在虚拟状态信息中的虚拟SEPC寄存器
    + 因为当VMM使用自己的sret指令返回到Guest时，它需要将真实的程序计数器设置成Guest操作系统想要的程序计数器值
    + （因为稍后Guest代码会在硬件上执行，因此依赖硬件上的程序计数器）
    + 所以在一个非常短的时间内，真实的SEPC寄存器与虚拟的SEPC寄存器值是一样的
      + 同时，当VMM返回到虚拟机时，还需要切换Page table
+ Guest中的用户代码，如果是普通的指令，就直接在硬件上执行
  + 当Guest中的用户代码需要执行系统调用时，会通过执行ECALL指令触发trap
  + 而这个trap会走到VMM中
    + （因为ECALL也是个privileged指令）
  + VMM可以发现当前在虚拟状态信息中记录的mode是User mode
    + 并且发现当前执行的指令是ECALL
    + 之后VMM会更新虚拟状态信息以模拟一个真实的系统调用的trap状态
  + 比如说，它将设置虚拟的SEPC为ECALL指令所在的程序地址
    + （执行sret指令时，会将程序计数器的值设置为SEPC寄存器的值。这样，当Guest执行sret指令时，可以从虚拟的SEPC中读到正确的值）
    + 将虚拟的mode更新成Supervisor
    + 将虚拟的SCAUSE设置为系统调用
    + 将真实的SEPC设置成虚拟的STVEC寄存器
      + STVEC保存的是trap函数的地址，将真实的SEPC设置成STVEC这样当VMM执行sret指令返回到Guest时，可以返回到Guest的trap handler
      + Guest执行系统调用以为自己通过trap走到了Guest内核，但是实际上却走到了VMM
      + 这时VMM需要做一些处理，让Guest以及之后Guest的所有privileged指令都看起来好像是Guest真的走到了Guest内核
    + 之后调用sret指令跳转到Guest操作系统的trap handler，也就是STVEC指向的地址
## Trap-and-Emulate --- Page Table
有关Trap and Emulate的实现还有两个重要的部分
+ 一个是Page Table
+ 另一个是外部设备
--------------------------------------------------------------
+ Page Table包含了两个部分
  + 第一个部分是Guest操作系统在很多时候会修改SATP寄存器，当然这会变成一个trap走到VMM，之后VMM可以接管
    + 但是我们不想让VMM只是简单的替Guest设置真实的SATP寄存器
      + 因为这样的话Guest就可以访问任意的内存地址，而不只是VMM分配给它的内存地址
      + 所以我们不能让Guest操作系统简单的设置SATP寄存器
    + 但是我们的确又需要为SATP寄存器做点什么，
      + 为我们需要让Guest操作系统觉得Page Table被更新了
      + 此外，当Guest上的软件运行了load或者store指令时，或者获取程序指令来执行时
        + 我们需要数据或者指令来自于内存的正确位置，也就是Guest操作系统认为其PTE指向的内存位置
        + 所以当Guest设置SATP寄存器时，真实的过程是:
          + 不能直接使用Guest操作系统的Page Table
          + VMM会生成一个新的Page Table来模拟Guest操作系统想要的Page Table
    + 所以现在的Page Table翻译过程略微有点不一样
      + 首先是Guest kernel包含了Page Table，但是这里是将Guest中的虚拟内存地址映射到了Guest的物理内存地址
      + Guest物理地址是VMM分配给Guest的地址空间，例如32GB
      + 并且VMM会告诉Guest这段内存地址从0开始，并一直上涨到32GB
      + 但是在真实硬件上，这部分内存并不是连续的
      + 所以我们不能直接使用Guest物理地址，因为它们不对应真实的物理内存地址
    + 相应的，VMM会为每个虚拟机维护一个映射表
      + 将Guest物理内存地址映射到真实的物理内存地址
        + 我们称之为主机物理内存地址
      + 这个映射表与Page Table类似，对于每个VMM分配给Guest的Guest物理内存Page，都有一条记录表明真实的物理内存Page是什么
+ 当Guest向SATP寄存器写了一个新的Page Table时
  + 在对应的trap handler中，VMM会创建一个Shadow Page Table，Shadow Page Table的地址将会是VMM向真实SATP寄存器写入的值
  + Shadow Page Table由上面两个Page Table组合而成，所以它将gva映射到了hpa
  + Shadow Page Table是这么构建的：
    + 从Guest Page Table中取出每一条记录，查看gpa
    + 使用VMM中的映射关系，将gpa翻译成hpa
    + 再将gva和hpa存放于Shadow Page Table
  + 在创建完之后，VMM会将Shadow Page Table设置到真实的SATP寄存器中，再返回到Guest内核中
  + （Guest里面看到的Page Table就是一个正常的Page Table，而Guest通过SATP寄存器指向的Page Table，将虚拟内存地址翻译得到的又是真实的物理内存地址）
+ 所以，Guest kernel认为自己使用的是一个正常的Page Table
  + 但是实际的硬件使用的是Shadow Page Table
  + 这种方式可以阻止Guest从被允许使用的内存中逃逸
  + Shadow Page Table只能包含VMM分配给虚拟机的主机物理内存地址
  + Guest不能向Page Table写入任何VMM未分配给Guest的内存地址
  + 这是VMM实现隔离的一个关键部分
+ Shadow Page Table是实现VMM时一个比较麻烦的地方
  + 除了设置SATP寄存器，Guest操作系统还有另一种方式可以与Page Table进行交互
  + XV6有时候会直接修改属于自己的Page Table Entry，或者读取PTE中的dirty bit
    + 如果软件更改了PTE，RISC-V不会做任何事情
    + 如果修改了PTE，RISC-V并不承诺可以立即观察到对于PTE的修改
      + 在修改那一瞬间，你完全是不知道PTE被修改了
        + 这里主要对比的是privileged指令
        + 因为如果在用户空间执行了privileged指令，会立刻触发trap
        + 而这里修改PTE不会有任何的额外的动作
    + 如果你修改PTE并且希望MMU可以看到这个改动
      + 需要执行sfence.vma指令
      + 这个指令会使得硬件注意到你对Page Table的修改
  + 所以如果你要自己写一个VMM，在RISC-V上的VMM会完全忽略Guest对于PTE的修改
    + 但是你知道Guest在修改完PTE之后将会执行sfence.vma指令，并且这是一个privileged指令
    + 因为它以s开头，所以这条指令会通过trap走到VMM，VMM就可以知道sfence.vma指令被执行了
    + 之后VMM会重新扫描Guest的当前Page Table，查找更新了的Page Table Entry
    + 如果修改合法的话，VMM会将修改体现在Shadow Page Table中
      + 并执行真实的sfence.vma指令来使得真实的硬件注意到Shadow Page Table的改动
      + 最后再会返回到Guest操作系统中
## Trap-and-Emulate --- Devices
虚拟机需要能够至少使得Guest认为所有它需要的外部设备是存在的
这里人们通常会使用三种策略:
------------------------------
第一种是:模拟一些需要用到的并且使用非常广泛的设备，例如磁盘
+ 也就是说，Guest并不是拥有一个真正的磁盘设备，只是VMM使得与Guest交互的磁盘看起来好像真的存在一样
+ 这里的实现方式是:
  + Guest操作系统仍然会像与真实硬件设备交互一样，通过Memory Map控制寄存器与设备进行交互
  + 通常来说，操作系统会假设硬件已经将自己的控制寄存器映射到了内核地址空间的某个地址上
  + 在VMM中不会映射这些内存地址对应的Page，相应的会将这些Page设置成无效
  + 这样当Guest操作系统尝试使用UART或者其他硬件时，一访问这些地址就会通过trap走到VMM
  + VMM查看指令并发现Guest正在尝试在UART发送字符或者从磁盘中读取数据
  + VMM中会对磁盘或者串口设备有一些模拟，通过这些模拟，VMM知道如何响应Guest的指令，之后再恢复Guest的执行
+ 这就是我们之前基于QEMU介绍XV6时，QEMU实现UART的方式
+ 在之前的介绍中，并没有UART硬件的存在
  + 但是QEMU模拟了一个UART来使得XV6正常工作
+ 这是一种常见的实现方式，但是这种方式可能会非常的低效
  + 因为每一次Guest与外设硬件的交互，都会触发一个trap
  + 但是对于一些低速场景，这种方式工作的较好
  + 如果你的目标就是能启动操作系统并使得它们完全不知道自己运行在虚拟机上，你只能使用这种策略
---------------------------------
第二种策略是:提供虚拟设备，而不是模拟一个真实的设备
+ 通过在VMM中构建特殊的设备接口，可以使得Guest中的设备驱动与VMM内支持的设备进行高效交互
+ 现在的Guest设备驱动中可能没有Memory Mapped寄存器了
  + 但是相应的在内存中会有一个命令队列
  + Guest操作系统将读写设备的命令写到队列中
  + 在XV6中也使用了一个这种方式的设备驱动，在XV6的virtio_disk.c文件中
  + 可以看到一个设备驱动尝试与QEMU实现的虚拟磁盘设备交互
  + 在这个驱动里面要么只使用了很少的，要么没有使用Memory Mapped寄存器，所以它基本不依赖trap，相应的它在内存中格式化了一个命令队列
  + 之后QEMU会从内存中读取这些命令
    + 但是并不会将它们应用到磁盘中
    + 而是将它们应用到一个文件，对于XV6来说就是fs.image
+ 这种方式比直接模拟硬件设备性能要更高，因为你可以在VMM中设计设备接口使得并不需要太多的trap
-------------------------------
第三个策略:对于真实设备的pass-through，这里典型的例子就是网卡
+ 现代的网卡具备硬件的支持，可以与VMM运行的多个Guest操作系统交互
+ 你可以配置你的网卡，使得它表现的就像多个独立的子网卡，每个Guest操作系统拥有其中一个子网卡
+ 经过VMM的配置，Guest操作系统可以直接与它在网卡上那一部分子网卡进行交互，并且效率非常的高
+ 在这种方式中，Guest操作系统驱动可以知道它们正在与这种特别的网卡交互
## 硬件对虚拟机的支持
+ 为什么Intel和其他的硬件厂商会为虚拟机提供直接的硬件支持呢？
  + 首先虚拟机应用的非常广泛，硬件厂商的大量客户都在使用虚拟机
  + 其次，Trap and Emulate虚拟机方案中，经常会涉及到大量高成本的trap，所以这种方案性能并不特别好
  + 第三个原因:RISC-V非常适合Trap and Emulate虚拟机方案，但是Intel的x86处理器的一些具体实现使得它可以支持虚拟化，但是又没那么容易
    + 所以Intel也有动力来修复这里的问题，因为它的很多客户想要在x86上运行VMM
+ 当使用这种新的硬件支持的方案时
  + VMM会使用真实的控制寄存器
  + 而当VMM通知硬件切换到Guest mode时，硬件里还会有一套完全独立，专门为Guest mode下使用的虚拟控制寄存器
  + 在Guest mode下可以直接读写控制寄存器，但是读写的是寄存器保存在硬件中的拷贝，而不是真实的寄存器
+ 硬件会对Guest操作系统的行为做一些额外的操作，以确保Guest不会滥用这些寄存器并从虚拟机中逃逸
  + 在这种硬件支持的虚拟机方案中，存在一些技术术语:
    + Guest mode被称为non-root mode
    + Host mode中会使用真实的寄存器，被称为root mode
  + 所以，硬件中保存的寄存器的拷贝，或者叫做虚拟寄存器是为了在non-root mode下使用
    + 真实寄存器是为了在root mode下使用
+ 现在，当我们运行在Guest kernel时，可以在不触发任何trap的前提下执行任何privileged指令
  + 比如说如果想读写STVEC寄存器，硬件允许我们直接读写STVEC寄存器的non-root拷贝
  + 这样，privileged指令可以全速运行，而不用通过trap走到VMM
  + 这对于需要触发大量trap的代码，可以运行的快得多
+ 现在当VMM想要创建一个新的虚拟机时，VMM需要配置硬件
  + 在VMM的内存中，通过一个结构体与VT-x硬件进行交互
  + 这个结构体称为VMCS（注，Intel的术语，全称是Virtual Machine Control Structure）
  + 当VMM要创建一个新的虚拟机时，它会先在内存中创建这样一个结构体
    + 并填入一些配置信息和所有寄存器的初始值
    + 之后VMM会告诉VT-x硬件说我想要运行一个新的虚拟机
    + 并且虚拟机的初始状态存在于VMCS中
    + Intel通过一些新增的指令来实现这里的交互
      + VMLAUNCH
        + 这条指令会创建一个新的虚拟机
        + 可以将一个VMCS结构体的地址作为参数传给这条指令，再开始运行Guest kernel
      + VMRESUME
        + 在某些时候，Guest kernel会通过trap走到VMM
        + 然后需要VMM中需要通过执行VMRESUME指令恢复代码运行至Guest kernel
      + VMCALL
        + 这条新指令在non-root模式下使用
        + 它会使得代码从non-root mode中退出，并通过trap走到VMM
+ 通过硬件的支持，Guest现在可以在不触发trap的前提下，直接执行普通的privileged指令
  + 但是还是有一些原因需要让代码执行从Guest进入到VMM中
    + 其中一个原因是调用VMCALL指令
    + 另一个原因是设备中断
      + 例如定时器中断会使得代码执行从non-root模式通过trap走到VMM
  + 所以通常情况下设备驱动还是会使得Guest通过trap走回到VMM
    + 这表示着Guest操作系统不能持续占有CPU，每一次触发定时器中断，VMM都会获取控制权
    + 如果有多个Guest同时运行，它们可以通过定时器中断来分时共享CPU
    + （类似于线程通过定时器中断分时共享CPU一样）
+ VT-x机制中的另外一大部分是对于Page Table的支持
  + 当我们在Guest中运行操作系统时，我们仍然需要使用Page Table
  + 首先Guest kernel还是需要属于自己的Page Table，并且会想要能够加载CR3寄存器，这是Intel中类似于SATP的寄存器
  + VT-x使得Guest可以加载任何想要的值到CR3寄存器，进而设置Page Table
  + 而硬件也会执行Guest的这些指令
    + 现在Guest kernel可以在不用通过trap走到VMM再来加载Page Table
+ 但是我们也不能让Guest任意的修改它的Page Table
  + 因为如果这样的话，Guest就可以读写任意的内存地址
  + 所以VT-x的方案中，还存在另一个重要的寄存器：
    + EPT（Extended Page Table）
    + EPT会指向一个Page Table
    + 当VMM启动一个Guest kernel时，VMM会为Guest kernel设置好EPT
    + 并告诉硬件这个EPT是为了即将运行的虚拟机准备的
+ 之后，当计算机上的MMU在翻译Guest的虚拟内存地址时
  + 它会先根据Guest设置好的Page Table，将Guest虚拟地址（gva）翻译到Guest 物理地址（gha）
  + 之后再通过EPT，将Guest物理地址（gha）翻译成主机物理地址（hpa）
  + 硬件会为每一个Guest的每一个内存地址都自动完成这里的两次翻译
  + EPT使得VMM可以控制Guest可以使用哪些内存地址
  + Guest可以非常高效的设置任何想要的Page Table，因为它现在可以直接执行privileged指令
    + 但是Guest能够使用的内存地址仍然被EPT所限制，而EPT由VMM所配置
    + 所以Guest只能使用VMM允许其使用的物理内存Page
    + （EPT类似于19.4中的Shadow Page Table）















