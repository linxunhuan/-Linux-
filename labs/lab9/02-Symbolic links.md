# 符号链接
## 官方题目
+ 在本练习中，您将向 xv6 添加符号链接
+ 符号链接（或软链接）通过路径名引用链接文件
  + 打开符号链接时，内核会按照链接找到引用的文件
+ 符号链接类似于硬链接，但硬链接仅限于指向同一磁盘上的文件，而符号链接可以跨磁盘设备
+ 虽然 xv6 不支持多个设备，但实现此系统调用是了解路径名查找工作原理的一个很好的练习
### 你的工作
+ 您将实现symlink(char *target, char *path) 系统调用，该调用在 path 处创建一个指向 target 所指文件的新符号链接
+ 有关更多信息，请参阅手册页 symlink
+ 要进行测试，请将 symlinktest 添加到 Makefile 并运行它
+ 当测试产生以下输出（包括用户测试成功）时，您的解决方案即完成:
```shell
$ symlinktest
Start: test symlinks
test symlinks: ok
Start: test concurrent symlinks
test concurrent symlinks: ok
$ usertests
...
ALL TESTS PASSED
$ 
```
## 官方提示
+ 首先，为symlink创建一个新的系统调用号
  + 在user/usys.pl、user/user.h中添加一个条目
  + 并在kernel/sysfile.c中实现一个空的sys_symlink
+ 向 kernel/stat.h添加一个新文件类型 ( T_SYMLINK ) 来表示符号链接
+ 向 kernel/fcntl.h 添加一个新标志 ( O_NOFOLLOW )，该标志可与open系统调用一起使用
  + 请注意，传递给 open 的标志使用按位或运算符组合，因此您的新标志不应与任何现有标志重叠
  + 这将允许您在将 user/symlinktest.c 添加到 Makefile 后对其进行编译
+ 实现symlink(target, path)系统调用以在 path 处创建一个指向 target 的新符号链接
  + 请注意，即使 target 存在，系统调用也能成功
  + 您需要选择某个地方来存储符号链接的目标路径，例如，在 inode 的数据块中
  + symlink应返回一个表示成功 (0) 或失败 (-1) 的整数，类似于link和unlink
+ 修改open系统调用以处理路径引用符号链接的情况
  + 如果文件不存在，则open 必定会失败
  + 当进程在要open 的标志中指定O_NOFOLLOW时，open应该打开符号链接（而不是跟随符号链接）
+ 如果链接文件也是符号链接，则必须递归跟踪它，直到到达非链接文件
  + 如果链接形成循环，则必须返回错误代码
  + 如果链接深度达到某个阈值（例如 10），则可以通过返回错误代码来近似实现这一点
+ 其他系统调用（例如，link 和 unlink）不能遵循符号链接
  + 这些系统调用对符号链接本身进行操作
+ 您不必处理此实验室的目录的符号链接
## 解题步骤
+ 这一部分要求我们为xv6实现添加符号链接的系统调用
  + 那么首先我们需要理解符号链接是什么
    + 符号链接，也叫做软链接，可以理解为windows上面的快捷方式
    + 只是原始文件或文件夹的别名，访问或者修改符号链接会直接导向指向的文件或文件夹
+ 根据文档提示
  + 我们可以将原始文件的路径存放在链接文件的block中
  + 可以理解为软链接文件中存放着原始文件的路径
### 第一步：向 kernel/stat.h添加一个新文件类型 ( T_SYMLINK ) 来表示符号链接
```c
#define T_SYMLINK 4
```
### 第二步：向 kernel/fcntl.h 添加一个新标志 ( O_NOFOLLOW )
```c
#define O_NOFOLLOW 0x004
```
### 第三步：实现symlink(target, path)系统调
```c
uint64
sys_symlink(void){
  // 定义两个字符数组，分别用于存储目标路径和符号链接的路径
  char path[MAXPATH],target[MAXPATH];
  struct inode *ip;

  // 获取系统调用的参数
  // argstr(0, target, MAXPATH) 获取第一个参数（符号链接的目标路径）并保存在 target 数组中。
  // argstr(1, path, MAXPATH) 获取第二个参数（符号链接的路径）并保存在 path 数组中。
  // 如果任一参数获取失败，返回 -1 表示错误。
  if (argstr(0, target, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0)
    return -1;

  // 开始一个新的文件系统操作，防止文件系统状态被破坏
  begin_op();

  // 创建一个新的符号链接文件
  if ((ip = create(path, T_SYMLINK, 0, 0)) == 0) {
    end_op();
    return -1;
  }

  // 解锁 inode，并结束文件系统操作
  iunlockput(ip);
  end_op();

  return 0;
}
```
### 第四步：修改open系统调用以处理路径引用符号链接的情况
```c
  // 如果打开模式包含 O_CREATE，表示需要创建文件
  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);        // 锁定 inode 以进行操作
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);  // 解锁 inode 并释放引用
      end_op();
      return -1;
    }
  }

// 如果没有设置 O_NOFOLLOW，表示可以跟随符号链接进行操作
  if(!(omode & O_NOFOLLOW)){
    int i = 0;

    // 循环解析符号链接，最多解析 10 次
    for (;i<10 && ip->type==T_SYMLINK;++i){

       // 读取符号链接的目标路径，并检查读取是否成功
      if(readi(ip,0,(uint64)path,0,MAXPATH) == 0){
        iunlockput(ip);
        end_op();
        return -1;
      }

      // 解析符号链接指向的新路径
      if((dp = namei(path))== 0){
        iunlockput(ip);
        end_op();
        return -1;
      }
      
      iunlockput(ip);
      ip = dp;        // 更新当前的 inode 为符号链接解析后的 inode
      ilock(ip);
    }

    // 如果解析符号链接的次数超过 10 次，返回错误
    if(i == 10){
      iunlockput(ip);
      end_op();
      return -1;
      }
  }
```
### 第五步：一系列的调用
+ 修改kernel/syscall.h
<img src=".\picture\image4.png">

+ 修改user/usys.pl
<img src=".\picture\image5.png">

+ 修改user/user.h
<img src=".\picture\image6.png">

+ 修改kernel/syscall.c
<img src=".\picture\image7.png">

+ 修改Makefile
<img src=".\picture\image8.png">

## 测试结果
<img src=".\picture\image9.png">




























