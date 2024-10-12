# 实验环境配置：
教程，亲测有效：
https://blog.csdn.net/qq_40029067/article/details/136367397
我的配置：win11 + wsl2 + Ubuntu20
我自己踩的坑：
+ 有些教程说相关软件只有Ubuntu20版本有
+ 我自己的实验环境：
  + QEMU 4.2（4.2以下的都不能用）
  + riscv64 9.2

# 写代码前知识的铺垫
+ xv6 内核提供了 Unix 传统系统调用的一部分，下面内容可见于user/user.h文件中
```c
int fork(void);                          //创建一个新进程    返回子进程的PID
int exit(int) __attribute__((noreturn)); //终止当前进程      返回状态码
int wait(int*);                          //等待子进程终止    并获取其状态
int pipe(int*);                          //创建一个管道      
int write(int, const void*, int);        //write(fd, buf, n)从buf中写n个字节到文件
int read(int, void*, int);               //read(fd, buf, n)	从文件中读n个字节到buf
int close(int);                          // 关闭打开的文件
int kill(int);                           //结束进程
int exec(char*, char**);                 //执行一个新程序
int open(const char*, int);              //打开文件，flags 指定读/写模式
int mknod(const char*, short, short);    //创建设备文件
int unlink(const char*);                 //删除文件
int fstat(int fd, struct stat*);         //获取文件状态     返回文件信息
int link(const char*, const char*);      //创建一个硬链接   给 f1 创建一个新名字(f2)
int mkdir(const char*);                  // 创建目录
int chdir(const char*);                  //改变当前工作目录
int dup(int);                            //复制fd
int getpid(void);                        // 获取当前进程PID
char* sbrk(int);                         //为进程内存空间增加 n 字节
int sleep(int);                          //使进程休眠指定时间
int uptime(void);                        //获取系统运行时间
```
## 进程和内存
+ fork 创建的新进程被称为子进程
+ 子进程的内存内容同创建它的进程（父进程）一样
+ fork 函数在父进程、子进程中都返回（一次调用两次返回）
  + 父进程返回子进程的 pid（肯定不是0）
  + 子进程返回 0

## I/O 和文件描述符
+ 每个进程都有一个从0开始的文件描述符空间
  + 进程从文件描述符0读入（标准输入）
  + 从文件描述符1输出（标准输出）
  + 从文件描述符2输出错误（标准错误输出）

+ 系统调用 read 和 write 从文件描述符所指的文件中读或者写 n 个字节
  + read(fd, buf, n) 
    + 从 fd 读最多 n 个字节（fd 可能没有 n 个字节），将它们拷贝到 buf 中，然后返回读出的字节数
    + 当没有数据可读时，read 就会返回0，这就表示文件结束了
  + write(fd, buf, n) 
    + 写 buf 中的 n 个字节到 fd 并且返回实际写出的字节数
    + 如果返回值小于 n 那么只可能是发生了错误
    + 就像 read 一样，write 也从当前文件的偏移处开始写，在写的过程中增加这个偏移
+ 管道和临时文件有三个不同点
  + 首先，管道会进行自我清扫
    + 如果是 shell 重定向的话，我们必须要在任务完成后删除 /tmp/xyz
  + 第二，管道可以传输任意长度的数据
  + 第三，管道允许同步：
    + 两个进程使用一对管道来进行二者之间的信息传递
    + 每一个读操作都阻塞调用进程，直到另一个进程用 write 完成数据的发送


## 文件系统
+ 文件和目录：
  + 文件：
    + 文件就是一个简单的字节数组
  + 目录：
    + 目录包含指向文件和其他目录的引用
    + xv6 把目录实现为一种特殊的文件
    + 目录是一棵树，它的根节点是一个特殊的目录 root
      + /a/b/c 指向一个在目录 b 中的文件 c，而 b 本身又是在目录 a 中的，a 又是处在 root 目录下的
      + 不从 / 开始的目录表示的是相对调用进程当前目录的目录
      + 调用进程的当前目录可以通过 chdir 这个系统调用进行改变
+ fstat 可以获取一个文件描述符指向的文件的信息
  + 它填充一个 stat 的结构体，具体可在 kernel/stat.h 中查看具体定义：
```c
#define T_DIR     1   // Directory          目录
#define T_FILE    2   // File               文件
#define T_DEVICE  3   // Device             设备

struct stat {
  int dev;     // File system's disk device 文件系统的磁盘设备
  uint ino;    // Inode number              Inode编号
  short type;  // Type of file              文件类型
  short nlink; // Number of links to file   文件的链接数
  uint64 size; // Size of file in bytes     文件的字节大小
};
```