+ 题目：
  + 使用管道编写素数筛的并发版本
  + 这个想法来自 Unix 管道的发明者 Doug McIlroy
  + 本页中间的图片 和周围的文字解释了如何做到这一点
  + 您的解决方案应该在文件user/primes.c中
+ 提示：
  + 请小心关闭进程不需要的文件描述符，否则在第一个进程达到 35 之前，您的程序就会耗尽 xv6 的资源
  + 一旦第一个进程达到 35，它就应该等到整个管道终止，包括所有子进程、孙进程等。因此，主 primes 进程应该在所有输出都打印完毕后，以及所有其他 primes 进程都退出后才退出
  + 提示：当管道的写入端关闭时， 读取返回零
  + 最简单的方法是直接将 32 位（4 字节）int写入管道，而不是使用格式化的 ASCII I/O
  + 您应该只在需要时才在管道中创建流程
  + 将程序添加到Makefile 中的UPROGS中
+ 我自己的理解：
  + 这道题的考察点是通过一个算法，来锻炼在进程和管道之间的通信
  + 难点在：因为涉及到递归，提示里也明确告诉了，性能不够，会程序崩溃，所以需要及时把不需要的关闭了

思路：
+ 需要完成的就是小学的时候学会的素数筛选方法：
  + 首先排除2的倍数、再排除3的倍数、再排除5的倍数
+ 所以这里其实就是通过子进程，每一个子进程，都代表了一个倍数的去除。从而实现这个不断的筛选
  + 如下图所示：
<img src=".\picture\image3.png">



下面是我写这道题的答案：
```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void get_prime(int pleft[2]){
    int p;
    read(pleft[0], &p, sizeof(p));
    if (p == -1) exit(0);
    printf("prime %d\n", p);

    int pright[2];
    pipe(pright);
    
    if (fork()== 0){
        //子进程
        close(pright[1]);// 子进程只需要对输入管道 pright 进行读，而不需要写，所以关掉子进程的输入管道写文件描述符，降低进程打开的文件描述符数量
        close(pleft[0]);//关闭父进程的输入管道
        get_prime(pright);//子进程以父进程的输出管道作为输入
    } else{
        //父进程
        close(pright[0]);//父进程只需要对子进程的输入管道进行写而不需要读，所以关掉父进程的读文件描述符
        int buf;
        while (read(pleft[0], &buf, sizeof(buf))){
            if (buf % p != 0) {
                write(pright[1], &buf, sizeof(buf));
            }
        }
        buf = -1;
        write(pright[1], &buf, sizeof(buf));
        wait(0);
        exit(0);
    }
}

int main(){
    int p[2];
    pipe(p);

    if(fork() == 0){
        close(p[1]);
        get_prime(p);
        exit(0);
    }else{
        close(p[0]);
        int buf;
        for(buf = 2; buf < 100; buf++){
            write(p[1], &buf, sizeof(buf));
        }
        buf = -1;
        write(p[1], &buf, sizeof(buf));
    }
    wait(0);
    exit(0);
}
```
下面是运算结果，符合题目要求：
<img src=".\picture\image4.png">