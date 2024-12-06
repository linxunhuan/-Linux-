# 使用线程
## 官方介绍
+ 在此作业中，您将使用哈希表探索线程和锁的并行编程
+ 您应该在具有多个内核的真实 Linux 或 MacOS 计算机（不是 xv6，不是 qemu）上完成此作业
+ 大多数最新的笔记本电脑都具有多核处理器
+ 此作业使用 UNIX pthread线程库。您可以使用man pthreads从手册页中找到有关它的信息 ，也可以在网上查找
+ 文件notxv6/ph.c包含一个简单的哈希表
  + 如果从单个线程使用，则正确，但从多个线程使用时则不正确
  + 在您的主 xv6 目录（可能是~/xv6-labs-2020）中，输入以下内容：
```shell
$ make ph
$ ./ph 1
```
+ 请注意，要构建ph， Makefile 会使用操作系统的 gcc，而不是 6.S081 工具
+ ph 的参数指定在哈希表上执行 put 和 get 操作的线程数
+ 运行一段时间后，ph 1将产生类似以下的输出：
```shell
100000 puts, 3.991 seconds, 25056 puts/second
0: 0 keys missing
100000 gets, 3.981 seconds, 25118 gets/second
```
+ 您看到的数字可能与此示例输出相差两个或更多倍
+ 这取决于您的计算机运行速度、是否有多个核心以及是否正在忙于执行其他操作
+ ph运行两个基准测试
+ 首先，它通过调用put()将大量键添加到哈希表中，并打印每秒实现的放入次数
+ 然后，它使用get()从哈希表中获取键
  + 它打印由于放入而应该在哈希表中但缺失的键数（在本例中为零）
  + 并打印每秒实现的获取次数
+ 您可以通过为ph提供一个大于 1 的参数，让其同时从多个线程使用其哈希表
+ 尝试ph 2:
```shell
$ ./ph 2
100000 puts, 1.885 seconds, 53044 puts/second
1: 16579 keys missing
0: 16579 keys missing
200000 gets, 4.322 seconds, 46274 gets/second
```
+ ph 2输出 的第一行表明
  + 当两个线程同时向哈希表添加条目时，它们的总插入速率达到每秒 53,044 次
  + 这大约是运行 ph 1 的单个线程的速率的两倍
  + 这是一个大约 2 倍的出色“并行加速”，这是人们可能希望达到的（即，两倍的核心数量，每单位时间产生的工作量是两倍）
+ 但是，两行显示16579 个键缺失，这表明哈希表中本应有大量键不存在
  + 也就是说，put 应该将这些键添加到哈希表中,但出了问题
  + 请查看notxv6/ph.c，特别是put() 和insert()

## 官方提示：
+ 为了避免发生这种情况，请在 notxv6/ph.c中的put和get中插入 lock 和 unlock 语句
+ 以便两个线程中缺少的键数始终为 0
+ 相关的 pthread 调用是：
```c
pthread_mutex_t lock; // 声明一个锁
pthread_mutex_init(&lock, NULL); // 初始化锁
pthread_mutex_lock(&lock); // 获取锁
pthread_mutex_unlock(&lock); // 释放锁
```
+ 不要忘记调用pthread_mutex_init()
  + 先用 1 个线程测试您的代码，然后用 2 个线程测试
  + 它是否正确（即您是否消除了丢失的密钥？）？相对于单线程版本，双线程版本是否实现了并行加速（即每单位时间的总工作量更多）？
+ 在某些情况下，并发put()在哈希表中读取或写入的内存没有重叠，因此不需要锁定来相互保护
+ 您可以更改ph.c以利用这种情况来获得某些put()的并行加速吗？
+ 提示：每个哈希桶一个锁怎么样？
+ 修改代码，使某些put操作并行运行，同时保持正确性
+ 当make grade表示您的代码通过了ph_safe和ph_fast测试时，您就大功告成了
+ ph_fast测试要求两个线程每秒产生的 put 次数至少是单个线程的 1.25 倍

## 解题思路
### 第一步： 为每个散列桶定义一个锁，将五个锁放在一个数组中，并进行初始化
```c
pthread_mutex_t lock[NBUCKET] = { PTHREAD_MUTEX_INITIALIZER }; // 每个散列桶一把锁
```
### 第二步：在put函数中对insert上锁
```c
if(e){
    // update the existing key.
    e->value = value;
} else {
    pthread_mutex_lock(&lock[i]);
    // the new is new.
    insert(key, value, &table[i], table[i]);
    pthread_mutex_unlock(&lock[i]);
}
```
## 实验结果
<img src=".\picture\image2.png">























































