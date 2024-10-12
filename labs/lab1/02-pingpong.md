+ 题目：
  + 编写一个程序，使用 UNIX 系统调用通过一对管道在两个进程之间“pingpong”一个字节，每个方向一个
    + 父进程应向子进程发送一个字节
      + 子进程应打印“<pid>: received ping”
      + 其中 <pid> 是其进程 ID
      + 将管道上的字节写入父进程，然后退出
    + 父进程应从子进程读取该字节
      + 打印“<pid>: received pong”，然后退出
  + 您的解决方案应位于文件user/pingpong.c中

+ 提示：
  + 使用管道创建管道
  + 使用fork创建一个子项
  + 使用read从管道读取，使用 write写入管道
  + 使用getpid查找调用进程的进程 ID
  + 将程序添加到Makefile 中的UPROGS中
  + xv6 上的用户程序可用的库函数有限。您可以在 user/user.h中查看列表
    + 源代码（系统调用除外）位于user/ulib.c、user/printf.c和user/umalloc.c中

下面按照提示,依次进行学习：
+ 提示里介绍的文件，已放在 01-user 的文件夹下，已经加入详细的自己的理解


下面就是我写这道题的答案：
```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char **argv) {
    // 创建管道会得到一个长度为 2 的 int 数组
	// 其中 0 为用于从管道读取数据的文件描述符，1 为用于向管道写入数据的文件描述符
    int pp2c[2], pc2p[2];
    pipe(pp2c); // 创建用于 父进程 -> 子进程 的管道
    pipe(pc2p); // 创建用于 子进程 -> 父进程 的管道

    if (fork() != 0) {//父进程
        write(pp2c[1],"!",1); //1.父进程首先发出该字节
        char buf;
        read(pc2p[0], &buf, 1); //2. 父进程发送完，开始等待子进程的回复
        printf("%d: received pong\n", getpid());
        wait(0);
    } else { //子进程
        char buf;
        read(pp2c[0], &buf, 1); //3. 子进程读取管道，收到父进程的字节数据
        printf("%d: received ping\n", getpid());
        write(pc2p[1], &buf, 1); //4. 子进程回复一个字节
    }
    exit(0);
}
```
下面是我的运行结果，符合题目要求：
<img src=".\picture\image2.png">
