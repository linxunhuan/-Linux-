# 操作系统结构
Kernel:

+  维护数据来管理每一个用户空间进程
+  维护了大量的数据结构来帮助它管理各种各样的硬件资源，以供用户空间的程序使用
+  通常会有文件系统实现类似文件名，文件内容，目录的东西，并理解如何将文件存储在磁盘中
+  不同的进程需要不同数量的内存，Kernel会复用内存、划分内存，并为所有的进程分配内存
-----

学习操作系统的矛盾点：

+ 操作系统既高效又易用
  + 高效通常意味着操作系统需要在离硬件近的low-level进行操作
  + 易用则要求操作系统为应用程序提供抽象的high-level可移植接口
+ 强大的操作系统服务和简单的接口
  + 强大的操作系统才能分担运行应用程序的负担
  + 给程序员简单的内核接口，不然他们不会用
+ 权限与安全
  + 希望给与应用程序尽可能多的灵活性
  + 不想要程序员能直接访问到硬件，干扰到其他的应用程序，或者干扰操作系统的行为
+ 其他
  + 线程之间的交互
  + 现在电子设备的愈发强大
--------------

文件描述符：

+ 本质上对应了内核中的一个表单数据
+ 内核维护了每个运行进程的状态，内核会为每一个运行进程保存一个表单，表单的key是文件描述符
  + 这个表单让内核知道，每个文件描述符对应的实际内容是什么
+ 每个进程都有自己独立的文件描述符空间：
  + 如果运行了两个不同的程序，对应两个不同的进程，如果它们都打开一个文件
  + 它们或许可以得到相同数字的文件描述符，但是因为内核为每个进程都维护了一个独立的文件描述符空间，这里相同数字的文件描述符可能会对应到不同的文件

-----------
# exec, wait系统调用
shell脚本里，有一个命令是echo
```shell
$ echo a b c 
abc
```
这个学过Linux的都知道吧


现在又有一个c语言的，文件名为exec的代码：
```c
//exec.c:replace a process with an executable file
#include "kernel/types.h"
#include "user/user.h"
int
main(){
    //定义数组
    char *argv[] = { "echo", "this","is","echo",0 };
    
    //exec的作用：操作系统从名为echo的文件中加载指令到当前的进程中，并替换了当前进程的内存
    exec("echo", argv);
    
    printf("exec failed!\n");
    exit(0);
}
```
+ exec会完全替换当前进程的内存，相当于当前进程不复存在了
+ exec只会当出错时，才会返回
+ 这个程序执行到exec的时候，成功运行，所以就没有返回
+ 也就是说，没有执行那个printf
<img src=".\picture\image(1).png">




下面又是一个c语言，文件名为fork/exec的程序
```c
#include"user/user.h"
//forkexec.c: fork then exec
int
main(){
    int pid, status;
    
    //用fork(),创建一个子进程
    pid = fork();
    
    if(pid == 0){
        
        char *argv[]={ "echo","THIS","IS","ECHO",0 };
        exec("echo"，argv);
        printf("exec failed!\n");
        exit(1);
    
    } else {
        
        printf("parent waiting\n");
        wait(&status);
        printf("the child exited with status &d\n", status);
        exit(0);
    }
}
```
+ 父进程是整个程序
+ 子进程就是fork()创建的
+ 子进程本来创建了，pid就是0，但结果执行到了exec
  + 子进程没了，那pid就不是0了
  + 子进程没了，那回到了父进程，所以从else下面的开始
+ wait系统调用，使得父进程可以等待任何一个子进程返回
  + 这里wait的参数status，是一种让退出的子进程以一个整数（32bit的数据）的格式与等待的父进程通信方式
+ 如果一个程序成功的退出了，那么exit的参数会是0
所以，最终结果，如图所示：
<img src=".\picture\image(2).png">
+ 但换言之，如果exec没有正常执行：

```c
#include "kernel/types.h"
#include "user/user.h"
//forkexec.c: fork then exec
int
main( ){
    int pid,status;
    pid = fork();
    if(pid == 0){
        
        char *argv[]={ "echo","THIS","IS","ECHO"，0 };
        exec("xklsdksdjkecho",argv);
        printf("exec failed!\n");
        exit(1);
    }else {
        
        printf("parent waiting\n");
        wait(&status);
        printf("the child exited with status sd\n",status);
    }
}
```
可以看到，和上一个代码的区别，就是为了让exec执行错误
所以这一次的结果就和上面不一样
![alt text](image3.png)

# I/O 重定向
```c
//redirect.c: run a command with output redirected
int
main(){
    int pid;
    pid = fork();
    if(pid == 0){
        close(1);
        
        open("output.txt",O_WRONLY|O_CREATE);
        char *argv[]={ "echo","this","is","redirected","echo",0 };
        exec("echo"，argv);
        
        printf("exec failed!\n");
        exit(1);
    }else{
        wait((int *)0);
    }
    exit(0);
}
```
这段代码，没有返回值；但是打开openput.txt文件，还是有对应的内容
+ 演示了分离fork和exec的好处：
  + fork和exec是分开的系统调用
  + 意味着，在子进程中有一段时间，fork返回了，但是exec还没有执行，子进程仍然在运行父进程的指令
  + 所以这段时间，尽管指令是运行在子进程中，但是这些指令仍然是父进程的指令，所以父进程仍然可以改变东西
  + 这里fork和exec之间的间隔，提供了Shell修改文件描述符的可能


