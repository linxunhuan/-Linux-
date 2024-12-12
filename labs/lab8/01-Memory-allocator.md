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
### 第一步：修改kmem定义为一个数组，包含NCPU个元素，即每个CPU对应一个
```c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];
```
### 第二步：修改kinit
+ 为所有锁初始化以“kmem”开头的名称
+ 该函数只会被一个CPU调用
+ freerange调用kfree将所有空闲内存挂在该CPU的空闲列表上
```c
void kinit() {
  int id;

  // 循环遍历所有 CPU 核心
  for(id = 0; id < NCPU; id++) {
    // 为每个 CPU 核心初始化一个锁
    // 每个锁的名字是 "kmem"，并将其绑定到 kmem[id].lock
    initlock(&kmem[id].lock, "kmem");
  }

  freerange(end, (void*)PHYSTOP);
}
```
### 第三步：修改kfree
+ 使用cpuid()和它返回的结果时必须关中断（题目里的提示）
```c
void kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree"); 


  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off(); // 关闭中断，防止在修改 freelist 时发生中断，保证操作的原子性

 
  int cpu_id = cpuid();

  // 锁住当前 CPU 的内存管理数据结构，防止并发访问导致错误
  acquire(&kmem[cpu_id].lock);

  // 将当前释放的内存页（r）添加到对应 CPU 的空闲内存链表中
  r->next = kmem[cpu_id].freelist;  // 将 freelist 的第一个元素指向当前释放的页
  kmem[cpu_id].freelist = r;        // 更新 freelist 为当前页，表示它是空闲的第一个页

  // 解锁当前 CPU 的内存管理数据结构
  release(&kmem[cpu_id].lock);

  pop_off();  // 恢复中断，允许中断发生
}
```
### 第四步：修改kalloc
+ 使得在当前CPU的空闲列表没有可分配内存时窃取其他内存的
```c
void *
kalloc(void)
{
  struct run *r; 

  push_off();
  int cpu_id = cpuid();  
  acquire(&kmem[cpu_id].lock);  // 获取当前 CPU 的内存池锁，以保护对 freelist 的操作

  // 尝试从当前 CPU 的内存池中获取一个空闲内存页
  r = kmem[cpu_id].freelist;
  if(r) {
    // 如果当前 CPU 有空闲内存页，更新 freelist，指向下一个空闲页
    kmem[cpu_id].freelist = r->next;
  } else {
    // 如果当前 CPU 没有空闲内存页，尝试从其他 CPU 的内存池中借一个
    int antid;  //用于遍历其他 CPU 的内存池
    for(antid = 0; antid < NCPU; antid++) {
      if(antid == cpu_id) {
        continue;  // 跳过当前 CPU 的内存池，已经在上面处理过
      }
      acquire(&kmem[antid].lock);  // 获取其他 CPU 内存池的锁

      // 尝试从该 CPU 的内存池中获取一个空闲页
      r = kmem[antid].freelist;
      if(r) {
        // 如果找到了空闲页，更新 freelist，并释放该 CPU 的锁
        kmem[antid].freelist = r->next;
        release(&kmem[antid].lock);
        break;  // 找到一个空闲页后，跳出循环
      }
      release(&kmem[antid].lock);  // 如果没有找到空闲页，释放锁并继续遍历其他 CPU
    }
  }

  release(&kmem[cpu_id].lock); 
  pop_off(); 

  if(r) {
    memset((char*)r, 5, PGSIZE);  
  }
  return (void*)r;  
}
```
## 测试结果
<img src=".\picture\image4.png">
<img src=".\picture\image5.png">
<img src=".\picture\image6.png">