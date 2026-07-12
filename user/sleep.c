#include "kernel/types.h"  
#include "user/user.h" 

//命令如 sleep 10,包含两个参数，每个参数是一个字符串
int main(int arg_count, char *arg_vector[])
{
  int ticks;//存放时间的临时变量

  if (arg_count != 2) {
    fprintf(2, "Usage: sleep <ticks>\n");
    exit(1);
  }

  //无负数
  if(arg_vector[1][0] == '-'){
    fprintf(2, "sleep: invalid tick count\n");
    exit(1);
  }

  ticks = atoi(arg_vector[1]);

  //无非数字字符
  for (int i = 0; arg_vector[1][i] != '\0'; i++) {
      if (arg_vector[1][i] < '0' || arg_vector[1][i] > '9') {
          fprintf(2, "sleep: invalid tick count\n");
          exit(1);
      }
  }

  pause(ticks);
  exit(0);
}
