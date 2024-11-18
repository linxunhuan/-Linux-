# RISC-V vs x86
|RISC-V|x86|
|-----|----|
|精简指令集|复杂指令集|
|趋向于完成更简单的工作，相应的也消耗更少的CPU执行时间|很多指令都做了不止一件事情。每一条都执行了一系列复杂的操作并返回结果|
|唯一的一款开源指令集||
# gdb和汇编代码执行
```c
int sum_to(int n){
    int acc =;
    for(int i=0;i<= n;i++){
        acc += i;
        return acc;
    }
}
```
上边的c语言代码对应的汇编如下面这样
```s
sum_to:
    mv t0,a0        # t0 <- a0
    li a0,0         # a0 <- 0
loop:
    add a0,a0,t0    # a0 <- a0 + t0
    addi t0,t0,-1   # t0 <- t0 - 1
    bnez t0,loop    # if t0 != 0;pc <- loop
    ret
```
+ 如果在自己的计算机编写同样的C代码并编译，你得到的极有可能是差别较大的汇编代码
+ 原因：
  + 当将C代码编译成汇编代码时，现代的编译器会执行各种各样的优化
+ 例如
  + 当在gdb中做debug的时候，有时候会看到gdb提示你说某些变量被优化掉了
  + 这意味着编译器决定了自己不再需要那个变量，变量以及相关的信息会在某个时间点删掉

----------------
（下面是演示操作）
+ 回到函数sum_to，我们看一下如何在gdb中检查这个函数。首先是要启动QEMU
<img src=".\picture\image6.png">

+ 在另一个窗口打开gdb
<img src=".\picture\image7.png">

+ gdb中输入tui enable可以打开源代码展示窗口，
+ sum_to的代码现在都位于内核中，在sum_to中设置一个断点
  + 然后继续代码的执行，代码在断点处停住
<img src=".\picture\image8.png">

+ gdb窗口的左上角是程序计数器，我们可以看到当前的值是0x800065e2
  + 如果我们去kernel.asm中，查找这个地址，我们可以看到这个地址就是sum_to函数的起始地址
<img src=".\picture\image9.png">

+ 如果代码出现了问题，在gdb中看到的地址，你可以直接在kernel.asm找到具体的行，分析问题的原因，然后再向相应的地址设置断点
+ 在gdb中输入layout asm，可以在tui窗口看到所有的汇编指令
+ 再输入layout reg可以看到所有的寄存器信息
<img src=".\picture\image10.png">

+  之后持续的单步执行代码，直到函数返回
+  如果你关心你设置了哪些断点，或着你跟踪代码的时候迷糊了
   +  可以在gdb中输入info breakpoints，你可以看到所有设置了的断点
   +  甚至可以看到这个断点已经被命中了几次
<img src=".\picture\image11.png">

# RISC-V寄存器
<img src=".\picture\image12.png">

+ 这个表里面是RISC-V寄存器
+ 寄存器是CPU或者处理器上，预先定义的可以用来存储数据的位置
+ 寄存器之所以重要是因为汇编代码并不是在内存上执行，而是在寄存器上执行
  + 也就是说，当我们在做add，sub时，我们是对寄存器进行操作
  + 所以通常看到的汇编代码中的模式是：
    + 通过load将数据存放在寄存器中，这里的数据源可以是来自内存，也可以来自另一个寄存器
    + 之后在寄存器上执行一些操作，如果对操作的结果关心的话，会将操作的结果store在某个地方
      + 这里的目的地可能是内存中的某个地址，也可能是另一个寄存器
  + 这就是通常使用寄存器的方法
--------
+ 寄存器是用来进行任何运算和数据读取的最快的方式，这就是为什么使用它们很重要，也是为什么我们更喜欢使用寄存器而不是内存
+ 当我们调用函数时，你可以看到这里有a0 - a7寄存器
+ 通常我们在谈到寄存器的时候，我们会用它们的ABI名字
  + 不仅是因为这样描述更清晰和标准
  + 同时也因为在写汇编代码的时候使用的也是ABI名字
+ 第一列中的寄存器名字并不是超级重要，它唯一重要的场景是在RISC-V的Compressed Instruction中
+ 基本上来说，RISC-V中通常的指令是64bit，但是在Compressed Instruction中指令是16bit
+ 在Compressed Instruction中我们使用更少的寄存器，也就是x8 - x15寄存器
+ 为什么s1寄存器和其他的s寄存器是分开的
  + 因为s1在Compressed Instruction是有效的
  + 而s2-11却不是
  + 除了Compressed Instruction，寄存器都是通过它们的ABI名字来引用
--------------
+ a0到a7寄存器是用来作为函数的参数
+ 如果一个函数有超过8个参数，我们就需要用内存
+ 从这里也可以看出，**当可以使用寄存器的时候,我们不会使用内存，我们只在不得不使用内存的场景才使用它**
+ 表单中的第4列，Saver列
  + 它有两个可能的值Caller，Callee
    + Caller Saved寄存器在函数调用的时候不会保存
    + Callee Saved寄存器在函数调用的时候会保存
+ 这里的意思是，一个Caller Saved寄存器可能被其他函数重写
+ 假设我们在函数a中调用函数b，任何被函数a使用的并且是Caller Saved寄存器，调用函数b可能重写这些寄存器
+ 我认为一个比较好的例子就是Return address寄存器（注，保存的是函数返回的地址）
  + 你可以看到ra寄存器是Caller Saved，这一点很重要，它导致了当函数a调用函数b的时侯，b会重写Return address
  + 所以基本上来说:
    + 任何一个Caller Saved寄存器，作为调用方的函数要小心可能的数据可能的变化
    + 任何一个Callee Saved寄存器，作为被调用方的函数要小心寄存器的值不会相应的变化
+ 所有的寄存器都是64bit,各种各样的数据类型都会被改造的可以放进这64bit中
  + 比如说我们有一个32bit的整数
    + 取决于整数是不是有符号的，会通过在前面补32个0或者1来使得这个整数变成64bit并存在这些寄存器中

# Stack
+ 栈之所以很重要的原因是，它使得我们的函数变得有组织，且能够正常返回
+ 下面是一个非常简单的栈的结构图
  + 其中每一个区域都是一个Stack Frame
  + 每执行一次函数调用就会产生一个Stack Frame
<img src=".\picture\image13.png">

+ 每一次我们调用一个函数，函数都会为自己创建一个Stack Frame，并且只给自己用
+ 函数通过移动Stack Pointer来完成Stack Frame的空间分配
+ 对于Stack来说，是从高地址开始向低地址使用,所以栈总是向下增长
+ 当我们想要创建一个新的Stack Frame的时候，总是对当前的Stack Pointer做减法
+ 一个函数的Stack Frame包含了保存的寄存器，本地变量，并且，如果函数的参数多于8个，额外的参数会出现在Stack中
  + 所以Stack Frame大小并不总是一样，即使在这个图里面看起来是一样大的
+ 不同的函数有不同数量的本地变量，不同的寄存器，所以Stack Frame的大小是不一样的
+ 但是有关Stack Frame有两件事情是确定的：
  + Return address总是会出现在Stack Frame的第一位
  + 指向前一个Stack Frame的指针也会出现在栈中的固定位置
+ 有关Stack Frame中有两个重要的寄存器
  + 第一个是SP（Stack Pointer），它指向Stack的底部并代表了当前Stack Frame的位置
  + 第二个是FP（Frame Pointer），它指向当前Stack Frame的顶部
+ 因为Return address和指向前一个Stack Frame的的指针都在当前Stack Frame的固定位置，所以可以通过当前的FP寄存器寻址到这两个数据
+ 我们保存前一个Stack Frame的指针的原因是为了让我们能跳转回去
  + 所以当前函数返回时，我们可以将前一个Frame Pointer存储到FP寄存器中
  + 所以我们使用Frame Pointer来操纵我们的Stack Frames，并确保我们总是指向正确的函数
+ Stack Frame必须要被汇编代码创建
  + 所以是编译器生成了汇编代码，进而创建了Stack Frame
  + 所以通常，在汇编代码中，函数的最开始你们可以看到Function prologue，之后是函数的本体，最后是Epilogue
----------
+ 再回到汇编本身
```s
sum_to:
    mv t0,a0        # t0 <- a0
    li a0,0         # a0 <- 0
loop:
    add a0,a0,t0    # a0 <- a0 + t0
    addi t0,t0,-1   # t0 <- t0 - 1
    bnez t0,loop    # if t0 != 0;pc <- loop
    ret
```
+ 之前的sum_to函数中，只有函数主体，并没有Stack Frame的内容
+ 它这里能正常工作的原因是它足够简单，并且它是一个leaf函数
+ leaf函数是指不调用别的函数的函数，它的特别之处在于它不用担心保存自己的Return address或者任何其他的Caller Saved寄存器，因为它不会调用别的函数
+ 而另一个函数sum_then_double就不是一个leaf函数了，这里你可以看到它调用了sum_to
<img src=".\picture\image14.png">

+ 所以在这个函数中，需要包含prologue
<img src=".\picture\image15.png">

+ 这里我们对Stack Pointer减16，这样我们为新的Stack Frame创建了16字节的空间
+ 之后我们将Return address保存在Stack Pointer位置
+ 之后就是调用sum_to并对结果乘以2。最后是Epilogue
<img src=".\picture\image16.png">

+ 这里首先将Return address加载回ra寄存器
  + 通过对Stack Pointer加16来删除刚刚创建的Stack Frame
  + 最后ret从函数中退出
+ 如果删除掉Prologue和Epilogue
  + 先在修改过的sum_then_double设置断点，然后执行sum_then_double
<img src=".\picture\image17.png">

+ 可以看到现在的ra寄存器是0x80006392，它指向demo2函数
  + 也就是sum_then_double的调用函数
  + 之后执行代码，调用了sum_to
<img src=".\picture\image18.png">

+ ra寄存器的值被sum_to重写成了0x800065f4，指向sum_then_double
  + 在函数sum_then_double中调用了sum_to，那么sum_to就应该要返回到sum_then_double
+ 之后执行代码直到sum_then_double返回
  + 因为没有恢复sum_then_double自己的Return address
    + 现在的Return address仍然是sum_to对应的值，现在就会进入到一个无限循环中

---------------
+ 接下来是一些C代码
```c
int dummymain(int argc, char *argv[]){
    int i = 0;
    for(;i< argc;i++){
        printf("Argument %d:%s\n",i,argv[i]);
    }
    return 0;
}

void demo4(){
    char *args[]={"foo","bar","baz"};
    int result = dummymain(sizeof(args)/sizeof(args[0]), args);
    if(result < 0){
        panic("Demo 4");
    }
}
```
+ demo4函数里面调用了dummymain函数。在dummymain函数中设置一个断点
<img src=".\picture\image19.png">

+ 在dummymain函数中。如果在gdb中输入info frame，可以看到有关当前Stack Frame许多有用的信息
<img src=".\picture\image20.png">

+ Stack level 0，表明这是调用栈的最底层
+ pc，当前的程序计数器
+ saved pc，demo4的位置，表明当前函数要返回的位置
+ source language c，表明这是C代码
+ Arglist at，表明参数的起始地址。当前的参数都在寄存器中，可以看到argc=3，argv是一个地址

+ 如果输入backtrace（简写bt）可以看到从当前调用栈开始的所有Stack Frame

<img src=".\picture\image21.png">

+ 如果对某一个Stack Frame感兴趣，可以先定位到那个frame再输入info frame
+ 假设对syscall的Stack Frame感兴趣
<img src=".\picture\image22.png">

# Struck
+ struct在内存中是一段连续的地址
  + 如果有一个struct，并且有f1，f2，f3三个字段
<img src=".\picture\image23.png">
+ 创建这样一个struct时，内存中相应的字段会彼此相邻
+ 可以认为struct像是一个数组，但是里面的不同字段的类型可以不一样

+ 可以将struct作为参数传递给函数
```c
struct Person {
    int id;
    int age;
    // char *name;
}
void printPerson(struct Person *p){
    printf("Person 名d(%d)\n"，p->id,p->age);
// printf("Name:$s:%d(%d)\n"，p->name,p->id, p->age);
}
```
+ 这里有一个名字是Person的struct，它有两个字段
+ 将这个struct作为参数传递给printPerson并打印相关的信息
+ 我们在printPerson中设置一个断点，当程序运行到函数内部时打印当前的Stack Frame
<img src=".\picture\image24.png">

+ 我们可以看到当前函数有一个参数p
+ 打印p可以看到这是struct Person的指针，打印p的反引用可以看到struct的具体内容
<img src=".\picture\image25.png">














