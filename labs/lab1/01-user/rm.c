//#include "kernel/types.h"
//#include "kernel/stat.h"
//#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    fprintf(2, "Usage: rm files...\n");
    
    // 退出程序，返回状态码1表示错误
    exit(1);
  }

  for(i = 1; i < argc; i++){
    // 尝试删除文件，如果删除失败，unlink函数返回-1
    if(unlink(argv[i]) < 0){
      fprintf(2, "rm: %s failed to delete\n", argv[i]);
      break;
    }
  }

  exit(0);
}
