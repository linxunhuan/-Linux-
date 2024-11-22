# 消除 sbrk() 中的分配
## 官方介绍
+ 您的首要任务是从 sbrk(n) 系统调用实现中删除页面分配
  + 即 sysproc.c 中的 sys_sbrk() 函数
+ sbrk(n) 系统调用将进程的内存大小增加 n 个字节
  + 然后返回新分配区域的起始位置（即旧大小）
+ 您的新 sbrk(n) 应该只是将进程的大小（myproc()->sz）增加 n 并返回旧大小
+ 它不应该分配内存
  + 因此您应该删除对 growproc() 的调用（但您仍然需要增加进程的大小！）
-----------------
+ 试猜一下这种修改的结果会是什么：会破坏什么？
+ 进行此修改，启动 xv6，然后在 shell 中 输入echo hi 。您应该会看到类似以下内容：
<img src=".\picture\image1.png">

+ “usertrap(): ...”消息来自 trap.c 中的用户陷阱处理程序
  + 它捕获了一个它不知道如何处理的异常
  + 确保您了解此页面错误发生的原因
  + “stval=0x0..04008”表示导致页面错误的虚拟地址是 0x4008

## 解题思路
+ 题目说的：
  + （myproc()->sz）增加 n 
  + 删除对 growproc() 的调用
```c
// sysproc.c
uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  
  myproc()->sz += n;
  // if(growproc(n) < 0)
  //   return -1;
  
  return addr;
}
```

## 运行结果
<img src=".\picture\image2.png">



