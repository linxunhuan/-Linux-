# 屏障
## 官方题目
+ 在本作业中，您将实现一个屏障：
  + 应用程序中所有参与线程都必须等待的点，直到所有其他参与线程也到达该点
  + 您将使用 pthread 条件变量，这是一种类似于 xv6 的睡眠和唤醒的序列协调技术
+ 你应该在真实的计算机（不是 xv6，不是 qemu）上完成这个作业
+ 文件notxv6/barrier.c包含一个损坏的屏障
```shell
$ make barrier
$ ./barrier 2
barrier: notxv6/barrier.c:42: thread: Assertion `i == t' failed.
```
+ 指定在屏障上同步的线程数（ barrier.c中的nthread）
+ 每个线程执行一个循环
+ 在每次循环迭代中，一个线程调用barrier()，然后休眠随机微秒数
+ 断言触发，因为一个线程在另一个线程到达屏障之前离开了屏障
+ 期望的行为是每个线程在barrier()中阻塞，直到所有nthreads都调用了 barrier()
## 官方提示
+ 您的目标是实现所需的屏障行为
+ 除了您在ph作业中看到的锁定原语之外，您还需要以下新的 pthread 原语
```c
pthread_cond_wait(&cond, &mutex); // 在 cond 上进入睡眠状态，释放锁互斥锁，唤醒时获取
pthread_cond_broadcast(&cond); // 唤醒在 cond 上睡眠的每个线程
```
+ pthread_cond_wait在调用时释放互斥锁，并在返回之前重新获取互斥锁
+ 我们已为您提供barrier_init()
+ 您的任务是实现 barrier()以避免发生恐慌
  + 我们已为您定义了struct barrier；其字段供您使用。
+ 有两个问题使您的任务变得复杂：
  + 您必须处理一系列屏障调用
    + 我们将每个调用称为一轮
    + bstate.round记录当前轮次
    + 每次所有线程都到达屏障时， 您都应该增加bstate.round 
  + 您必须处理一个线程在其他线程退出屏障之前在循环中竞争的情况
    + 特别是，您将从一轮到下一轮重复使用bstate.nthread变量
    + 确保离开屏障并在循环中竞争的线程不会在上一轮仍在使用 bstate.nthread时增加它
## 解题步骤
+ 只要保证下一个round的操作不会影响到上一个还未结束的round中的数据就可
```c
static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //
  pthread_mutex_lock(&bstate.barrier_mutex);
  if(++bstate.nthread < nthread) {
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  } else {
    bstate.nthread = 0;
    bstate.round++;
    pthread_cond_broadcast(&bstate.barrier_cond);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}
```
## 运行结果
<img src=".\picture\image3.png">





















































































































