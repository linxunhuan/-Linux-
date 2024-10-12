//作用：将所有命令行参数按顺序输出，并在每个参数之间添加一个空格，最后一个参数后添加一个换行符

//#include "kernel/types.h"
//#include "kernel/stat.h"
//#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;

  // 循环遍历所有命令行参数，从索引1开始，因为索引0是程序的名称
  for(i = 1; i < argc; i++){

    // 将当前参数写入标准输出（文件描述符1），长度为参数字符串的长度
    write(1, argv[i], strlen(argv[i]));
    
    
    if(i + 1 < argc){
      write(1, " ", 1);
    } else {
      write(1, "\n", 1);
    }
  }
  exit(0);
}
/*
+ 这里我详细举一个例子介绍：
  + 相当于，此时我在命令行输入：echo ./test Hello World
    + argc 的值为 4，因为有 4 个参数（包括程序名称）
    + argv 数组的内容为：["./test", "Hello", "World"]
  + 执行for循环执行2次
    + 第一次迭代 (i = 1)：
      + argv[1] 是 "Hello"，写入标准输出
      + 因为 i + 1 < argc，写入一个空格
      + 输出：Hello
    +  第二次迭代 (i = 2)：
       +  argv[2] 是 "World"，写入标准输出
       +  因为 i + 1 < argc，写入一个空格
       +  输出：Hello World
*/