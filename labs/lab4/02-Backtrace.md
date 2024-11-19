# 回溯
## 题目：
+ 对于调试来说，进行回溯通常很有用：在错误发生点上方的堆栈上的函数调用列表。
+ 在 kernel/printf.c 中实现 backtrace() 函数
  + 在 sys_sleep 中插入对此函数的调用，然后运行调用 sys_sleep 的 bttest
  + 您的输出应如下所示：
```s
backtrace:
0x0000000080002cda
0x0000000080002bb6
0x0000000080002898
```
+ bttest 退出 qemu 后
  + 在您的终端中：地址可能略有不同，但如果您运行 
    + addr2line -e kernel/kernel
    + （或 riscv64-unknown-elf-addr2line -e kernel/kernel）
  + 并剪切并粘贴上述地址，如下所示：
```s
$ addr2line -e kernel/kernel
    0x0000000080002de2
    0x0000000080002f4a
    0x0000000080002bfc
    Ctrl-D
```
+ 你应该看到类似这样的内容：
  + kernel/sysproc.c:74
  + kernel/syscall.c:224
  + kernel/trap.c:85
+ 编译器在每个堆栈帧中放入一个帧指针，该指针保存调用者的帧指针的地址
  + 您的回溯应该使用这些帧指针遍历堆栈并在每个堆栈帧中打印保存的返回地址
## 官方提示：
+ 将 backtrace 的原型添加到 kernel/defs.h，以便您可以在 sys_sleep 中调用 backtrace
+ GCC 编译器将当前执行函数的帧指针存储在寄存器 s0 中。将以下函数添加到 kernel/riscv.h：
```c
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x) );
  return x;
}
```
    并在 backtrace 中调用此函数读取当前帧指针。此函数使用内联汇编读取 s0
+ 这些讲义中有一张堆栈框架布局的图片
+ 请注意，返回地址位于距堆栈框架的框架指针的固定偏移量 (-8) 处
+ 而保存的框架指针位于距框架指针的固定偏移量 (-16) 处
+ xv6 在 xv6 内核中为每个堆栈分配一个页面，地址与 PAGE 对齐
+ 您可以使用 PGROUNDDOWN(fp) 和 PGROUNDUP(fp) 计算堆栈页面的顶部和底部地址（请参阅 kernel/riscv.h）
+ 这些数字有助于 backtrace 终止其循环
+ 一旦你的回溯工作正常，在 kernel/printf.c 中的 panic 中调用它，这样你就可以在 kernel panic 时看到它的回溯

## 解题思路
### 第一步：添加 backtrace 功能，打印出调用栈，用于调试
<img src=".\picture\image2.png">

### 第二步：将提示给的函数添加到kernel/riscv.h文件中
<img src=".\picture\image3.png">

### 第三步：完成backtrace函数
#### xv6堆栈和指针介绍
+ 这里是介绍：如何获取当前返回地址和下一个栈帧的地址
+ xv6堆栈结构如图所示：
<img src=".\picture\image4.png">

先说结论：
**下一个栈帧指针地址 = fp - 8**
返回值地址 = fp - 16
--------------
具体分析：
+ fp 指向当前栈帧的开始地址，sp 指向当前栈帧的结束地址
  + （栈从高地址往低地址生长，所以 fp 虽然是帧开始地址，但是地址比 sp 高）
+ 栈帧中从高到低第一个 8 字节 fp-8 是 return address——当前调用层应该返回到的地址
+ 栈帧中从高到低第二个 8 字节 fp-16 是 previous address——指向上一层栈帧的 fp 开始地址
+ 剩下的为保存的寄存器、局部变量等
+ 一个栈帧的大小不固定，但是至少 16 字节
+ 在 xv6 中，使用一个页来存储栈，如果 fp 已经到达栈页的上界，则说明已经到达栈底
-------------
+ 查看 call.asm
+ 一个函数的函数体最开始首先会扩充一个栈帧给该层调用使用，在函数执行完毕后再回收
```s
int g(int x) {
   0:	1141                  addi  sp,sp,-16  // 扩张调用栈，得到一个 16 字节的栈帧
   2:	e422                  sd    s0,8(sp)   // 将返回地址存到栈帧的第一个 8 字节中
   4:	0800                  addi  s0,sp,16
  return x+3;
}
   6:	250d                  addiw a0,a0,3
   8:	6422                  ld    s0,8(sp)   // 从栈帧读出返回地址
   a:	0141                  addi  sp,sp,16   // 回收栈帧
   c:	8082                  ret              // 返回
```
栈的生长方向是从高地址到低地址，所以扩张是 -16，而回收是 +16
### 写入backtrace
```c
void
backtrace(){
  printf("backtrace:\n");
  uint64 fp = r_fp();               // 获取当前栈帧的帧指针
  while (fp != PGROUNDUP(fp)){      
    uint64 ra = *(uint64*)(fp - 8); // 识别它是否到了最后一个栈帧
    printf("%p\n",ra);              // 输出当前栈帧的返回值
    fp = *(uint64*)(fp - 16);       // 获取上一栈帧的帧指针
  }
}
```
### 第四步：回到sys_sleep函数中调用backtrace()
<img src=".\picture\image5.png">

## 测试
<img src=".\picture\image6.png">







