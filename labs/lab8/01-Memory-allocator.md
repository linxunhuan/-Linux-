# 内存分配器
## 官方题目
+ 程序 user/kalloctest 强调了 xv6 的内存分配器：
  + 三个进程的地址空间不断增大和缩小，导致多次调用kalloc和kfree
  + kalloc和kfree 获得kmem.lock
  + kalloctest打印（作为“# fetch -and-add”） acquire中的循环迭代次数
    + 因为尝试获取另一个核心已经持有的锁，对于 kmem 锁和一些其他锁
    + acquire 中的循环迭代次数是锁争用的粗略衡量标准
    + 在您完成实验之前，kalloctest的 输出与此 类似 ：
```S
$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 83375 #acquire() 433015
lock: bcache: #fetch-and-add 0 #acquire() 1260
--- top 5 contended locks:
lock: kmem: #fetch-and-add 83375 #acquire() 433015
lock: proc: #fetch-and-add 23737 #acquire() 130718
lock: virtio_disk: #fetch-and-add 11159 #acquire() 114
lock: proc: #fetch-and-add 5937 #acquire() 130786
lock: proc: #fetch-and-add 4080 #acquire() 130786
tot= 83375
test1 FAIL
```
+ 对于每个锁， acquire0维护该锁的acquire调用次数，以及acquire循环尝试设置锁但失败的次数
+ kalloctest 调用一个系统调用，使内核打印 kmem 和 bcache 锁（这是本实验的重点）以及 5 个最有争用锁的计数
+ 如果存在锁争用，acquire循环迭代次数将很大
+ 系统调用返回 kmem 和 bcache 锁的循环迭代次数之和
-------------------------------------
+ 对于本实验，您必须使用专用的未加载多核计算机
  + 如果您使用的计算机正在执行其他操作，则 kalloctest 打印的计数将毫无意义
  + 您可以使用专用的 Athena 工作站或您自己的笔记本电脑，但不要使用拨号计算机
+ kalloctest 中锁争用的根本原因是kalloc()只有一个空闲列表，由一个锁保护
  + 要消除锁争用，您必须重新设计内存分配器以避免单个锁和列表
  + 基本思想是每个 CPU 维护一个空闲列表，每个列表都有自己的锁
  + 不同 CPU 上的分配和释放可以并行运行，因为每个 CPU 都将在不同的列表上操作
  + 主要的挑战是处理一个 CPU 的空闲列表为空，但另一个 CPU 的列表有空闲内存的情况
  + 在这种情况下，一个 CPU 必须“窃取”另一个 CPU 空闲列表的一部分
-------------------------------------
+ 您的工作是实现每个 CPU 的空闲列表，并在 CPU 的空闲列表为空时进行窃取
+ 您必须为所有锁指定以“kmem”开头的名称
  + 也就是说，您应该为每个锁调用 initlock ，并传递以“kmem”开头的名称
+ 运行 kalloctest 以查看您的实现是否减少了锁争用
+ 要检查它是否仍能分配所有内存，请运行usertests sbrkmuch
+ 您的输出将类似于下面显示的输出
  + 总体上 kmem 锁的争用大大减少，尽管具体数字会有所不同
  + 确保usertests中的所有测试都通过。make grade应该显示 kalloctests 通过
```S
$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 0 #acquire() 42843
lock: kmem: #fetch-and-add 0 #acquire() 198674
lock: kmem: #fetch-and-add 0 #acquire() 191534
lock: bcache: #fetch-and-add 0 #acquire() 1242
--- top 5 contended locks:
lock: proc: #fetch-and-add 43861 #acquire() 117281
lock: virtio_disk: #fetch-and-add 5347 #acquire() 114
lock: proc: #fetch-and-add 4856 #acquire() 117312
lock: proc: #fetch-and-add 4168 #acquire() 117316
lock: proc: #fetch-and-add 2797 #acquire() 117266
tot= 0
test1 OK
start test2
total free number of pages: 32499 (out of 32768)
.....
test2 OK
$ usertests sbrkmuch
usertests starting
test sbrkmuch: OK
ALL TESTS PASSED
$ usertests
...
ALL TESTS PASSED
$
```
## 官方提示
+ 您可以使用kernel/param.h 中的 常量NCPU
+ 让freerange将所有可用内存提供给运行freerange 的CPU 
+ 函数cpuid返回当前核心编号，但只有在关闭中断的情况下调用它并使用其结果才是安全的
  + 您应该使用 push_off()和pop_off()来关闭和打开中断
+ 请查看kernel/sprintf.c 中的snprintf函数，了解字符串格式化的思路
  + 不过，将所有锁都命名为“kmem”也是可以的
## 解题步骤
+ 总的来说，题目的要求：
  + 为每个CPU都维护一个空闲列表
  + 初始时将所有的空闲内存分配到某个CPU
  + 此后各个CPU需要内存时，如果当前CPU的空闲列表上没有，则窃取其他CPU的
    + 例如，所有的空闲内存初始分配到CPU0，当CPU1需要内存时就会窃取CPU0的
    + 而使用完成后就挂在CPU1的空闲列表，此后CPU1再次需要内存时就可以从自己的空闲列表中取
### 将kmem定义为一个数组，包含NCPU个元素，即每个CPU对应一个
























