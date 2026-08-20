#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

#define PGSIZE 4096

int is_secret_char(char c) {
  return (c >= 'a' && c <= 'z') ||
    (c >= 'A' && c <= 'Z') ||
    (c >= '0' && c <= '9');
}

int
main(int argc, char *argv[])
{
  // Your code here.
  int pages = 64;
  char* start = sbrk(0);

  for (int i = 0; i < pages; i++) {
    if (sbrk(PGSIZE) == (char*)-1) {
      exit(1);
    }
  }

  //查找"This may help."标记
  for (int i = 0; i < pages * PGSIZE - 20; i++) {
    //检查是否匹配 "This may help."
    if (start[i] == 'T' && start[i + 1] == 'h' &&
      start[i + 2] == 'i' && start[i + 3] == 's' &&
      start[i + 4] == ' ' && start[i + 5] == 'm' &&
      start[i + 6] == 'a' && start[i + 7] == 'y' &&
      start[i + 8] == ' ' && start[i + 9] == 'h' &&
      start[i + 10] == 'e' && start[i + 11] == 'l' &&
      start[i + 12] == 'p' && start[i + 13] == '.') {

      // 在"This may help."后面 16 字节处
      char* secret = start + i + 16;

      int len = 0;
      while (len < 100 && is_secret_char(secret[len])) {
        len++;
      }

      //以\0结尾且长度至少 1
      if (len >= 1 && secret[len] == '\0') {
        printf("%s\n", secret);
        exit(0);
      }
    }
  }
  exit(0);
}
