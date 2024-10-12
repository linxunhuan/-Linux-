+ 题目：
  + 编写一个简单版本的 UNIX find 程序：查找目录树中具有特定名称的所有文件
  + 您的解决方案应位于文件user/find.c中
+ 官方提示：
  + 查看 user/ls.c 了解如何读取目录
  + 使用递归让 find 进入子目录
  + 不要递归到“.”和“..”
  + 对文件系统的更改在 qemu 运行期间保持不变；要获得干净的文件系统，请运行make clean然后make qemu
  + 您需要使用 C 字符串。请参阅 K&R（C 书），例如第 5.5 节
  + 请注意，== 不会像 Python 那样比较字符串。请改用 strcmp()
  + 将程序添加到Makefile 中的UPROGS中
+ 我自己的理解：
  + 我庆幸在写这段代码之前，看了ls.c文件中的内容，我自己写的很多部分直接就是仿照在那里的
```c
//ls.c

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  // 定义一个静态缓冲区来存储格式化后的名称
  static char buf[DIRSIZ+1];
  char *p;

  // 找到路径中最后一个斜杠后的第一个字符
  // 从字符串的末尾开始向前移动
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;// 移动到最后一个斜杠后的字符

  // 如果名称的长度大于或等于 DIRSIZ，
  // 直接返回指向该名称的指针
  if(strlen(p) >= DIRSIZ)
    return p;
  
  // 将名称复制到缓冲区中
  memmove(buf, p, strlen(p));

  // 用空格填充缓冲区中剩余的空间
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  
  // 返回包含格式化名称的缓冲区
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  // 打开指定路径的文件或目录
  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  // 获取文件或目录的状态信息
  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  // 根据文件类型进行不同的处理
  switch(st.type){
  
  case T_FILE:// 如果是文件，输出文件的名称、类型、inode号和大小
    printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
    break;

  case T_DIR:// 如果是目录，检查路径长度是否超出缓冲区大小
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }

    // 将路径复制到缓冲区，并在末尾添加一个斜杠
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';

    // 读取目录中的每个条目
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;

    // 将目录项的名称复制到缓冲区，并添加空终止符
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;

    // 获取目录项的状态信息，如果失败则输出错误信息并继续
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }

      // 输出目录项的名称、类型、inode号和大小
      printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  // 如果没有提供命令行参数，默认列出当前目录的内容
  if(argc < 2){
    ls(".");// 调用 ls 函数列出当前目录
    exit(0);
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit(0);
}
```

下面是我自己写的答案：
```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
char* get_name(char* path){//获取当前文件名
    char * p;
    for(p = path+strlen(path); p >= path && *p != '/'; p--);
    p++;
    return p;
}
void find(char *path, char* str){
    char buf[512];//存储路径
    struct dirent de;//目录结构体
    struct stat st;//文件结构体
    int fd = open(path, 0);//0表示以标准模式(读写模式)打开
    if(fd < 0){//打开失败
        fprintf(2, "find: cannot open %s\n", path);
    	return;
    }

    if(fstat(fd, &st) < 0){//通过文件描述符将对应的文件信息放入文件结构体stat中,若失败则返回-1
        fprintf(2, "find: cannot stat %s\n", path);
    	close(fd);
    	return;
    }
    switch(st.type){

    	case T_FILE:
       	    if(!strcmp(str, get_name(path)))
       	        printf("%s\n",path);
       	    break;
       	case T_DIR:
    	    strcpy(buf, path);
       	    char *p = buf + strlen(buf);
    	    *p = '/';
       	    p++;
       	    while(read(fd, &de, sizeof de) == sizeof de){//使用read从目录文件中读取目录条目，处理目录中文件
    	    	if(de.inum == 0){continue;}//该目录条目为空或未使用
    	    	    
    	    	memmove(p, de.name, DIRSIZ);
     	    	p[DIRSIZ] = 0;
    	    	if(stat(buf, &st) < 0){//通过文件路径将对应的文件信息放入文件结构体stat中，若失败则返回-1
            	    printf("ls: cannot stat %s\n", buf);
            	    continue;
    	    	}
    	    	if(st.type == T_DEVICE || st.type == T_FILE){//判断为非目录文件
    	    	    if(!strcmp(str, get_name(buf)))
    	                printf("%s\n",buf);
    	    	}
    	    	else if(st.type == T_DIR && strcmp(".", get_name(buf)) && strcmp("..", get_name(buf)))//判定为子目录，递归处理，注意不要重复进入本目录以及父目录
    	    	    find(buf, str);
    	    }
    	    break;
    }
    close(fd);
    return;
}
int main(int argc, char *argv[]){
    if(argc == 3){
        find(argv[1], argv[2]);
    }else{
        printf("argument error\n");
    }
    exit(0);
}
```
下面是的运行截图，符合题目要求：
<img src=".\picture\image5.png">
