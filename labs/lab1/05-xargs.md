+ 题目：
  + 编写 UNIX xargs 程序的简单版本：从标准输入读取行并对每行运行一个命令，并将行作为参数提供给命令
  + 您的解决方案应位于文件user/xargs.c中
+ 官方提示：
  + 使用fork和exec在输入的每一行上调用命令
  + 在父进程中使用wait等待子进程完成命令
  + 要读取输入的各行，请一次读取一个字符，直到出现换行符（“\n”）
  + kernel/param.h 声明了 MAXARG，如果您需要声明一个 argv 数组，这可能会很有用
  + 将程序添加到Makefile 中的UPROGS中
  + 对文件系统的更改在 qemu 运行过程中保持不变；要获得干净的文件系统，请运行使清洁进而制作 qemu


+ 首先是比较意外的，因为确实在学Linux的时候没有学这个命令，此处放一下介绍
  + xargs 能够捕获一个命令的输出，然后传递给另外一个命令
  + 之所以能用到这个命令，关键是由于很多命令不支持|管道来传递参数，而日常工作中有有这个必要，所以就有了 xargs 命令
  + 例如：
```shell
find /sbin -perm +700 |ls -l       #这个命令是错误的
find /sbin -perm +700 |xargs ls -l   #这样才是正确的
```

下面是我的代码结果
```c
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[]){
    
    char *p[MAXARG];// 定义一个字符指针数组，用于存储参数
    int i;
    
    // 将命令行参数复制到 p 数组中，从第二个参数开始
    for(i=1; i < argc; i++){
        p[i-1] = argv[i];
    }
        
    // 为最后一个参数（从 gets 函数读取的输入）分配内存
    p[argc-1]  = malloc(512);
    p[argc] = 0;//将参数数组以 NULL 结尾

    while(gets(p[argc-1],512)){ //gets函数一次读取一行
        
        if(p[argc-1][0] == 0){break;}
        
        if(p[argc-1][strlen(p[argc-1])-1]=='\n'){// 如果有换行符，则去掉
            p[argc-1][strlen(p[argc-1])-1] = 0;
        } 

        if(fork()==0){
            exec(argv[1],p);
        }else{
            wait(0);
        }
    }
    exit(0);
}
```

下面是代码的运行结果，符合题目要求：
<img src=".\picture\image6.png">