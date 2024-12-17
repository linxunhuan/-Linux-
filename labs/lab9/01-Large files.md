# 大文件
## 官方题目
+ 在这个作业中，你将增加 xv6 文件的最大大小
+ 目前 xv6 文件限制为 268 个块，或 268*BSIZE 字节（在 xv6 中 BSIZE 为 1024）
+ 这个限制来自这样一个事实：xv6 inode 包含 12 个“直接”块号和一个“单间接”块号，后者指的是一个最多可容纳 256 个块号的块，总共 12+256=268 个块
+ bigfile命令创建其所能创建的最长的文件并报告其大小：
```shell
$ bigfile
..
wrote 268 blocks
bigfile: file is too small
$
```
+ 
+ 测试失败，因为bigfile期望能够创建一个包含 65803 个块的文件，但未修改的 xv6 将文件限制为 268 个块
+ 您将更改 xv6 文件系统代码以支持每个 inode 中的“双重间接”块，其中包含 256 个单间接块地址，每个单间接块最多可包含 256 个数据块地址
  + 结果是一个文件最多可包含 65803 个块，或 256*256+256+11 个块（11 个而不是 12 个，因为我们将为双重间接块牺牲一个直接块编号）
### 准备工作
+ mkfs程序创建 xv6 文件系统磁盘映像并确定文件系统总共有多少个块
+ 此大小由 kernel/param.h中的FSSIZE控制。您将看到此实验的存储库中的FSSIZE设置为 200,000 个块。您应该 在 make 输出中 看到来自mkfs/mkfs的以下输出：
```shell
nmeta 70 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 25) blocks 199930 total 200000
```
+ 此行描述了mkfs/mkfs构建 的文件系统：
  + 它有 70 个元数据块（用于描述文件系统的块）和 199,930 个数据块，总共 200,000 个块
+ 如果在实验过程中的任何时候你发现自己必须从头开始重建文件系统，你可以运行make clean来强制 make 重建 fs.img
### 看什么
+ 磁盘上的 inode 格式由fs.h 中的struct dinode定义
+ 您对NDIRECT、 NINDIRECT、MAXFILE以及struct dinode的addrs[]元素特别感兴趣
+ 查看 xv6 文本中的图 8.3，了解标准 xv6 inode 的图表
+ 在磁盘上查找文件数据的代码位于fs.c 中的bmap()中
+ 查看它并确保你理解它在做什么
+ 读取和写入文件时都会调用bmap() 
+ 写入时， bmap()会根据需要分配新块来保存文件内容，并在需要时分配间接块来保存块地址
+ bmap()处理两种块号
  + bn 参数是“逻辑块号”——文件内的块号，相对于文件开头
  + ip- >addrs[]中的块号和bread()的参数是磁盘块号
  + 您可以将bmap ()视为将文件的逻辑块号映射到磁盘块号
### 你的工作
+ 修改bmap()，使其除了直接块和单间接块之外，还实现双间接块
+ 您必须只有 11 个直接块，而不是 12 个，才能为新的双间接块腾出空间
  + 您不能更改磁盘上 inode 的大小
+ ip- >addrs[]的前 11 个元素应该是直接块
  + 第 12 个应该是单间接块（就像当前的块一样）
  + 第 13 个应该是您的新双间接块
+ 当bigfile写入 65803 个块并且usertests成功运行 时，您就完成了此练习：
```shell
$ bigfile
................................................................
wrote 65803 blocks
done; ok
$ usertests
...
ALL TESTS PASSED
$ 
```
+ bigfile至少需要一分半钟才能运行
## 官方提示
+ 确保您理解bmap()
  + 写出ip->addrs[]、间接块、双间接块及其指向的单间接块和数据块之间的关系图
  + 确保您理解为什么添加双间接块会使最大文件大小增加 256*256 个块（实际上是 -1，因为您必须将直接块的数量减少一个）
+ 考虑一下如何使用逻辑块号来索引双重间接块以及它指向的间接块
+ 如果更改NDIRECT的定义，则可能必须更改file.h中struct inode 中addrs[]的声明
  + 确保 struct inode和struct dinode在其addrs[]数组 中具有相同数量的元素
+ 如果您更改NDIRECT的定义，请确保创建一个新的fs.img
  + 因为mkfs使用NDIRECT来构建文件系统
+ 如果您的文件系统陷入不良状态（例如崩溃），请删除fs.img（从 Unix 而不是 xv6 执行此操作）
  + make 将为您构建一个新的干净的文件系统映像
+ 不要忘记对每个你 bread()过的块进行brelse()
+ 您应该仅根据需要分配间接块和双重间接块，就像原始的bmap()一样
+ 确保itrunc释放文件的所有块，包括双重间接块
## 解题步骤
### 第一步：在fs.h中添加宏定义
```c
// NDIRECT 定义了直接地址的数量，通常用于索引文件的直接块。
// 在此示例中，假设每个文件的 inode 中最多有 11 个直接指向数据块的地址。
#define NDIRECT 11

// NINDIRECT 定义了间接地址的数量。
// 间接地址是指一个数据块存储多个地址，每个地址指向另一个数据块。 
// BSIZE 是每个数据块的大小，sizeof(uint) 是单个地址的大小。
// 因此，NINDIRECT 计算的是一个数据块中能够存储多少个指向其他数据块的地址。
#define NINDIRECT (BSIZE / sizeof(uint))

// NDINDIRECT 定义了二级间接地址的数量。
// 二级间接地址是指一个数据块存储指向另一个间接地址块的地址。
// 每个二级间接地址块中存储 NINDIRECT 个地址，因此 NDINDIRECT 是 NINDIRECT 的平方。
#define NDINDIRECT ((BSIZE / sizeof(uint)) * (BSIZE / sizeof(uint)))

// MAXFILE 定义了一个文件中最多可以有多少个数据块。
// 这是一个包含直接地址、一级间接地址和二级间接地址的总数。
// 直接地址是 NDIRECT，一级间接地址是 NINDIRECT，二级间接地址是 NDINDIRECT。
#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT)

// NADDR_PER_BLOCK 定义了每个数据块中可以存储多少个地址。
// 每个数据块的大小是 BSIZE，单个地址的大小是 sizeof(uint)，
// 因此一个数据块可以存储 BSIZE / sizeof(uint) 个地址。
#define NADDR_PER_BLOCK (BSIZE / sizeof(uint))  // 一个块中的地址数量
```
### 第二步：由于NDIRECT定义改变，其中一个直接块变为了二级间接块，需要修改inode结构体中addrs元素数量
```c
// fs.h
struct dinode {
  ...
  uint addrs[NDIRECT + 2];   // Data block addresses
};

// file.h
struct inode {
  ...
  uint addrs[NDIRECT + 2];
};
```
### 第三步：修改bmap支持二级索引
```c
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    ...
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    ...
  }
  bn -= NINDIRECT;

  // 二级间接块的情况
  if(bn < NDINDIRECT) {
    int level2_idx = bn / NADDR_PER_BLOCK;  // 要查找的块号位于二级间接块中的位置
    int level1_idx = bn % NADDR_PER_BLOCK;  // 要查找的块号位于一级间接块中的位置
    // 读出二级间接块
    if((addr = ip->addrs[NDIRECT + 1]) == 0)
      ip->addrs[NDIRECT + 1] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    if((addr = a[level2_idx]) == 0) {
      a[level2_idx] = addr = balloc(ip->dev);
      // 更改了当前块的内容，标记以供后续写回磁盘
      log_write(bp);
    }
    brelse(bp);

    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[level1_idx]) == 0) {
      a[level1_idx] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}
```
### 第四步：修改itrunc释放所有块
```c
void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    ...
  }

  if(ip->addrs[NDIRECT]){
    ...
  }

  struct buf* bp1;
  uint* a1;
  if(ip->addrs[NDIRECT + 1]) {
    bp = bread(ip->dev, ip->addrs[NDIRECT + 1]);
    a = (uint*)bp->data;
    for(i = 0; i < NADDR_PER_BLOCK; i++) {
      // 每个一级间接块的操作都类似于上面的
      // if(ip->addrs[NDIRECT])中的内容
      if(a[i]) {
        bp1 = bread(ip->dev, a[i]);
        a1 = (uint*)bp1->data;
        for(j = 0; j < NADDR_PER_BLOCK; j++) {
          if(a1[j])
            bfree(ip->dev, a1[j]);
        }
        brelse(bp1);
        bfree(ip->dev, a[i]);
      }
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT + 1]);
    ip->addrs[NDIRECT + 1] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}
```


























































