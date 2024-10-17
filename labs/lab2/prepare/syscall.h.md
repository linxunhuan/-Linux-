```c
// System call numbers
#define SYS_fork    1//创建一个新进程
#define SYS_exit    2//终止当前进程
#define SYS_wait    3//等待子进程终止
#define SYS_pipe    4//创建一个管道，用于进程间通信
#define SYS_read    5//从文件描述符读取数据
#define SYS_kill    6// 向进程发送信号
#define SYS_exec    7//执行一个新程序
#define SYS_fstat   8//获取文件状态信息
#define SYS_chdir   9//改变当前工作目录
#define SYS_dup    10//复制文件描述符
#define SYS_getpid 11//获取当前进程ID
#define SYS_sbrk   12//调整进程的数据段大小
#define SYS_sleep  13//使进程休眠一段时间
#define SYS_uptime 14//获取系统运行时间
#define SYS_open   15//打开文件
#define SYS_write  16//向文件描述符写入数据
#define SYS_mknod  17//创建一个文件系统节点（如文件、设备）
#define SYS_unlink 18//删除文件
#define SYS_link   19//创建一个新的文件链接
#define SYS_mkdir  20//创建一个新目录
#define SYS_close  21//关闭文件描述符
```