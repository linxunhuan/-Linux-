## 题目：
+ 为了帮助您了解 RISC-V 页表，并且可能有助于未来的调试，您的首要任务是编写一个打印页表内容的函数
  + 定义一个名为vmprint() 的函数
  + 它应该接受一个pagetable_t参数，并以下面描述的格式打印该页表
  + 在 exec.c 中的return argc之前插入if(p->pid==1) vmprint(p->pagetable) ，以打印第一个进程的页表
+ 如果您通过了make grade的pte 打印输出测试，您将获得此作业的满分
## 官方提示
+ 您可以将 vmprint() 放在 kernel/vm.c 中
+ 使用文件 kernel/riscv.h 末尾的宏
+ freewalk 函数可能会有所启发
+ 在 kernel/defs.h 中定义 vmprint 的原型，以便您可以从 exec.c 调用它
+ 在您的 printf 调用中使用 %p 来打印出完整的 64 位十六进制 PTE 和地址，如示例中所示


## 做题思路：
### kernel/riscv.h 末尾的宏介绍：
```c
#define PGSIZE 4096 // bytes per page
#define PGSHIFT 12  // 页内偏移量

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4) // 1 -> user can access

// 将物理地址移动到 PTE 的正确位置
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// 从虚拟地址中提取三个 9 位页表索引
#define PXMASK          0x1FF // 9 bits
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)

// 比最高可能的虚拟地址多一个位
// MAXVA 实际上比 Sv39 允许的最大值小一位，以避免必须对设置了高位的虚拟地址进行符号扩展.
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs
```
### freewalk函数分析
```c
// 递归释放页表页面。所有页映射必须已被删除。
void
freewalk(pagetable_t pagetable)
{
  // 一个页表中有 2^9 = 512 个 PTE
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    
    // 检查该PTE是否有效且不是指向页
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // 该 PTE 指向低级页表
      uint64 child = PTE2PA(pte);
      
      // 递归调用freewalk函数，释放低级页表
      freewalk((pagetable_t)child);
      
      // 清楚该PTE
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      
      // 如果PTE有效但不是低级页表，抛出错误
      panic("freewalk: leaf");
    }
  }
  // 释放当前页表的内存
  kfree((void*)pagetable);
}
```
+ 根据书上介绍：
  + RISC-V 的逻辑地址寻址是采用三级页表的形式、
  + 9 bit 一级索引找到二级页表
  + 9 bit 二级索引找到三级页表
  + 9 bit 三级索引找到内存页
  + 最低 12 bit 为页内偏移（即一个页 4096 bytes）
+ 这道题需要模拟如上的 CPU 查询页表的过程，对三级页表进行遍历，然后按照一定格式输出
### vmprint函数
+ 这道题，通过改freewalk函数，freewalk是释放内存，把相关的，变成打印就可以了
```c
int
pgtblprint(pagetable_t pagetable, int depth){
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){  // 如果页表有效
      //按格式打印页表项
      printf("..");
      for(int j = 0; j < depth; j++){
        printf("..");
      }
      printf("%d :pte %p pa %p\n",i,pte,PTE2PA(pte));

      // 如果该节点不是叶节点，递归打印其子节点
      if((pte & (PTE_R | PTE_W | PTE_X)) == 0){
        uint64 child = PTE2PA(pte);
        pgtblprint((pagetable_t)child, depth + 1);
      }
    }
  }
  return 0; 
}

int vmprint(pagetable_t pagetable){
  printf("page table %p\n", pagetable);
  return pgtblprint(pagetable,0);
}
```
+ kernel/defs.h中增加vmprint()的声明
<img src=".\picture\image7.png">

+ 在kernel/exec.c中增加对vmprint()的调用
<img src=".\picture\image8.png">


## 遗憾
+ 正常编译结果完全符合系统要求
<img src=".\picture\image9.png">

+ 但是make grade的时候，一直显示超时。目前猜测，可能是因为qemu版本太低，所以时间超时
<img src=".\picture\image10.png">