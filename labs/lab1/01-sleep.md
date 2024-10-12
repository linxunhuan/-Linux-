+ 题目：
  + 为 xv6实现 UNIX 程序sleep
  + 您的sleep应该暂停用户指定的滴答数
  + 滴答是 xv6 内核定义的时间概念，即定时器芯片两次中断之间的时间
  + 您的解决方案应该位于文件 user/sleep.c中
+ 官方的提示：
  + 在开始编码之前，请阅读xv6 书的第 1 章。[我已经在00里介绍了相关知识]
  + 查看user/中的一些其他程序 （例如，user/echo.c，user/grep.c和user/rm.c）以了解如何获取传递给程序的命令行参数
  + 如果用户忘记传递参数，sleep 应该打印一条错误消息
  + 命令行参数以字符串形式传递；您可以使用atoi将其转换为整数（参见 user/ulib.c）
  + 使用系统调用sleep。
  + 请参阅kernel/sysproc.c了解实现sleep系统调用（查找sys_sleep ）的 xv6 内核代码，参阅user/user.h了解可从用户程序调用的sleep 的 C 定义，参阅user/usys.S了解从用户代码跳转到内核执行sleep的汇编代码
  + 确保main调用exit()来退出程序。
  + 将您的睡眠程序添加到Makefile 中的UPROGS；完成后，make qemu将编译您的程序，您将能够从 xv6 shell 运行它


下面按照提示,依次进行学习：
+ 提示里介绍的文件，已放在 01-user 的文件夹下，已经加入详细的自己的理解
+ 下面是sleep相关的：

```c
//kernel/sysproc.c 中 sleep系统调用的xv6 内核代码
//通过检查时钟滴答数来实现进程的休眠，同时确保在休眠期间如果进程被杀死能够及时退出

uint64
sys_sleep(void)
{
  int n;       //传递过来的休眠时间
  uint ticks0; //记录当前的时钟滴答数

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}
```
下面就是我第一题的答案：
```c
#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"

int
main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Usage: sleep <ticks>\n");
    }
    sleep(atoi(argv[1]));
    exit(0);
}
```
下面是程序的运行截图，符合题目要求：
<img src=".\picture\image1.png">