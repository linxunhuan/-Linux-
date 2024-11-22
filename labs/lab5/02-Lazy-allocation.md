# 惰性分配
## 官方介绍
+ 修改 trap.c 中的代码
  + 通过在错误地址映射新分配的物理内存页面
  + 然后返回用户空间以让进程继续执行
    + 来响应来自用户空间的页面错误
  + 您应该在产生“usertrap(): ...”消息的printf调用之前添加代码
  + 修改您需要的任何其他 xv6 内核代码，以使echo hi正常工作
## 官方提示
+ 您可以通过查看 usertrap() 中的 r_scause() 是否为 13 或 15 来检查错误是否为页面错误
+ r_stval()返回 RISC-V stval寄存器，其中包含导致页面错误的虚拟地址
+ 从 vm.c 中的 uvmalloc() 窃取代码，这是 sbrk() 调用的（通过 growproc()）。您需要调用 kalloc() 和 mappages()
+ 使用 PGROUNDDOWN(va) 将错误虚拟地址向下舍入到页面边界
+ uvmunmap() 会发生崩溃；如果某些页面未被映射，请对其进行修改以便不会崩溃
+ 如果内核崩溃，请在 kernel/kernel.asm 中查找 sepc
+ 使用pgtbl 实验室的vmprint函数打印页表的内容
+ 如果您看到错误“不完整的类型 proc”，请包含“spinlock.h”然后“proc.h”
## 解题思路
### 第一步：提示的前四步
+ 这一步解决的问题是：
  + 访问某个虚拟地址时，发现该地址：
    + 不在进程的页表中（没有映射到物理内存）
    + 映射存在但权限不足
+ 先展示题目中说的，需要参考的uvmalloc()
```c
// 为用户空间分配虚拟内存
// 参数：
// - pagetable: 页表指针
// - oldsz: 当前分配的内存大小
// - newsz: 需要分配的新内存大小
// 返回值：
// - 成功返回新的内存大小
// - 失败返回 0
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  // 如果新大小小于当前大小，不需要分配，直接返回当前大小
  if(newsz < oldsz)
    return oldsz;

  // 将 oldsz 向上对齐到页面边界
  oldsz = PGROUNDUP(oldsz);

  // 循环从 oldsz 分配到 newsz
  for(a = oldsz; a < newsz; a += PGSIZE){
    // 分配一页物理内存
    mem = kalloc();
    // 如果分配失败，释放已经分配的内存并返回 0
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    // 将分配的内存清零
    memset(mem, 0, PGSIZE);
    // 将物理页面映射到虚拟地址空间
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      // 如果映射失败，释放分配的内存并返回 0
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  // 返回新分配的内存大小
  return newsz;
}
```
------------------------
这是题目答案
```c
// 提示一：您可以通过查看 usertrap() 中的 r_scause() 是否为 13 或 15 来检查错误是否为页面错误
else if(r_scause() == 15 || r_scause() == 13) {
    
    // 提示二：r_stval()返回 RISC-V stval寄存器，其中包含导致页面错误的虚拟地址
    uint64 stval = r_stval();

    char *mem = kalloc();

    // 将分配的物理内存清零，确保新页面的内容初始化为 0
    memset(mem,0, PGSIZE);

    // 将虚拟地址映射到物理地址，并更新进程的页表
    // 提示四：使用 PGROUNDDOWN(va) 将错误虚拟地址向下舍入到页面边界
    if(mappages(p->pagetable, PGROUNDDOWN(stval),PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U) != 0){
      kfree(mem);
      p -> killed = 1;
    }
```
+ 这里多解释一句，两端代码的区别
  + uvmalloc()是在内存分配失败时清理已经分配的内存，需要确保资源的正确释放
    + 所以有函数uvmdealloc()的使用
  + 题目需要写的，是因为映射存在但权限不足，需要直接把这个进程杀死
    + 所以有p -> killed = 1

### 第二步：修改uvmunmap
+ 题目说：如果某些页面未被映射，会崩溃
+ 仔细一看，原来是题目自己写的panic
+ 那我把panic全跳了，不就行了
```c
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;          // 当前处理的虚拟地址。
  pte_t *pte;        // 页表项指针。

  // 检查传入的虚拟地址是否页对齐，如果未对齐则报错。
  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  // 遍历所有需要取消映射的页，按页大小逐一处理。
  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    // 获取虚拟地址 a 对应的页表项指针。如果未找到，说明页表存在问题，触发 panic。
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");

    // 检查页表项是否有效 (PTE_V)。如果无效，说明该页没有映射，触发 panic。
    if((*pte & PTE_V) == 0)
      continue;

    // 如果页表项只有有效位 (PTE_V)，而没有其他权限位，说明不是叶子节点，触发 panic。
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");

    // 如果 do_free 为真，则释放对应的物理页。
    if(do_free){
      // 从页表项中提取物理地址 (PTE2PA)。
      uint64 pa = PTE2PA(*pte);
      // 释放该物理页的内存。
      kfree((void*)pa);
    }

    // 将页表项清零，取消映射。
    *pte = 0;
  }
}
```



















