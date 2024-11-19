# 警告
## 题目介绍
+ 在本练习中，您将向 xv6 添加一项功能，该功能会在进程使用 CPU 时间时定期提醒它
  + 这可能对计算受限的进程很有用，这些进程希望限制它们占用的 CPU 时间
  + 或者对想要计算但也想要采取一些定期操作的进程很有用
  + 更一般地说，您将实现一种原始形式的用户级中断/故障处理程序；
    + 例如，您可以使用类似的东西来处理应用程序中的页面错误
    + 如果您的解决方案通过了警报测试和用户测试，则说明它是正确的
## 题目步骤
+ 您应该添加一个新的 sigalarm(interval, handler) 系统调用
+ 如果应用程序调用 sigalarm(n, fn)
  + 在程序消耗的每 n 个 CPU 时间“ticks”之后，内核应该导致调用应用程序函数 fn
+ 当 fn 返回时，应用程序应该从中断的地方恢复
  + ticks是 xv6 中相当随意的时间单位，由硬件计时器生成中断的频率决定
  + 如果应用程序调用 sigalarm(0, 0)，内核应该停止生成定期警报调用
+ 您将在 xv6 存储库中找到文件 user/alarmtest.c
  + 将其添加到 Makefile
  + 除非您添加了 sigalarm 和 sigreturn 系统调用（见下文），否则它无法正确编译
+ alarmtest 在 test0 中调用 sigalarm(2, periodic)
  + 要求内核每 2 个ticks,强制调用 periodic() 一次，然后旋转一段时间
  + 您可以在 user/alarmtest.asm 中看到 alarmtest 的汇编代码，这对调试很有帮助
  + 当 alarmtest 产生如下输出并且 usertests 也能正确运行时，您的解决方案是正确的：
```shell
$ alarmtest
test0 start
........alarm!
test0 passed
test1 start
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
test1 passed
test2 start
................alarm!
test2 passed
$ usertests
...
ALL TESTS PASSED
$
```
+ 完成后，您的解决方案将只有几行代码，但要正确完成可能有些困难
+ 我们将使用原始存储库中的 alarmtest.c 版本测试您的代码
+ 您可以修改 alarmtest.c 以帮助您进行调试，但请确保原始 alarmtest 表明所有测试都通过




















