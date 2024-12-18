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
+ xv6 文件系统中的每一个 inode 结构体中，采用了混合索引的方式记录数据的所在具体盘块号
+ 每个文件所占用的前 12 个盘块的盘块号是直接记录在 inode 中的（每个盘块 1024 字节）
  + 所以对于任何文件的前 12 KB 数据，都可以通过访问 inode 直接得到盘块号
  + 这一部分称为直接记录盘块
+ 对于大于 12 个盘块的文件
  + 大于 12 个盘块的部分，会分配一个额外的一级索引表（一盘块大小，1024Byte）
  + 用于存储这部分数据的所在盘块号
+ 由于一级索引表可以包含 BSIZE(1024) / 4 = 256 个盘块号
  + 加上 inode 中的 12 个盘块号
  + 一个文件最多可以使用 12+256 = 268 个盘块，也就是 268KB
+ inode 结构（含有 NDIRECT=12 个直接记录盘块，还有一个一级索引盘块
  + 后者又可额外包含 256 个盘块号
<img src=".\picture\image3.png">

+ 本 lab 的目标是通过为混合索引机制添加二级索引页，来扩大能够支持的最大文件大小
+ 在我们修改代码之前，xv6的文件系统限制为12个直接块和1个一级间接块
  + 最大文件大小为(12+256)*1024字节
+ 我们需要实现的是，将1个直接块替换为1个二级间接块
  + 其中，二级间接块的意思是，该块中存储256个一级简接块的块号
  + 也就是说，最大文件大小变为(11+256+256*256)*1024字节
----------------------------------
前三步的操作，其实对应了题目中的这两句：
+ 磁盘上的 inode 格式由fs.h 中的struct dinode定义
+ 您对NDIRECT、 NINDIRECT、MAXFILE以及struct dinode的addrs[]元素特别感兴趣
### 第一步:修改NDIRECT
```c
// fs.h
#define NDIRECT 11
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT)
```
### 第二步：修改dinode结构体
```c
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+2];   // Data block addresses
};
```
### 第三步：修改indoe结构体
```c
// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+2];
};
```
### 第四步：修改bmap函数
+ 参考一级间接块的处理方案:
  + 首先需要判断当前请求的文件块是第几块
    + 如果是二级间接块的话，那么需要获取两个偏移量
      + 分别是二级间接块的偏移量（bn / NINDIRECT1）
      + 该偏移量所指向一级间接块的偏移量（bn % NINDIRECT1）
    + 然后先看二级间接块是否存在，不存在则分配，并跑到对应的一级间接块中
    + 查看一级间接块是否存在，不存在则分配，最后跑到目标块中
    + 查看目标块是否存在，不存在则分配
```c

//  bn 参数是“逻辑块号”——文件内的块号，相对于文件开头
//  ip- >addrs[]中的块号和bread()的参数是磁盘块号
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
     // 检查 inode 中是否已经分配了单级间接块。如果没有分配，则分配一个新的间接块
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);

    // 从磁盘中读取该间接块的数据（包含指向数据块的地址）
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;// 将间接块数据转换为地址数组

    // 检查间接块中的指定位置（a[bn]）是否已经分配了数据块
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  // 如果逻辑块号 bn 大于等于 NINDIRECT，则进入双级间接块映射（doubly-indirect）。
  bn -= NINDIRECT;  // 减去单级间接块的偏移量，计算出双级间接映射中的偏移位置
  if(bn < NINDIRECT * NINDIRECT){

    // 检查 inode 中是否已经分配了双级间接块。如果没有分配，则分配一个新的双级间接块
    if((addr = ip->addrs[NDIRECT + 1]) == 0)
      ip->addrs[NINDIRECT + 1] = addr = balloc(ip->dev);


    // 从磁盘中读取该双级间接块（该块包含指向单级间接块的地址）
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    // 再次检查双级间接块中的指定位置（a[bn/NINDIRECT]）是否已经分配了数据块
    if((addr = a[bn / NINDIRECT]) == 0){
      a[bn / NINDIRECT] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);       // 释放缓冲区，不再需要该双级间接块
    bn %= NINDIRECT;  // 计算出单级间接块中的偏移位置

    // 从磁盘中读取该单级间接块的数据（包含指向数据块的地址）
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    // 检查单级间接块中的指定位置（a[bn]）是否已经分配了数据块
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }

    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}
```
### 第五步：修改itrunc函数
+ 题目里面说的：确保itrunc释放文件的所有块，包括双重间接块
```c
void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

 // 1. 释放 inode 中的直接块 (NDIRECT)
  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

// 2. 释放 inode 中的间接块 (NDIRECT)
  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

 // 3. 释放 inode 中的双重间接块 (NDIRECT+1)
  if(ip->addrs[NDIRECT + 1]){
    bp = bread(ip->dev, ip->addrs[NDIRECT + 1]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j]){
        struct buf *bp2 = bread(ip->dev, a[j]);
        uint *a2 = (uint*)bp2->data;

        for(int k = 0; k < NINDIRECT; k++){
          if(a2[k]){
            bfree(ip->dev, a2[k]);
          }  
        }
      brelse(bp2);  // 释放一级间接块的缓冲区
      bfree(ip->dev, a[j]);  // 释放一级间接块
      }
    }
    brelse(bp);                             // 释放二级间接块的缓冲区
    bfree(ip->dev, ip->addrs[NDIRECT + 1]); // 释放双重间接块本身
    ip->addrs[NDIRECT + 1] = 0;             // 重置双重间接块
  }

  ip->size = 0;
  iupdate(ip);
}
```