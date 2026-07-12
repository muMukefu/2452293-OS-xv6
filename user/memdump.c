#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data);

int
main(int argc, char *argv[])
{
  if(argc == 1){
    printf("Example 1:\n");
    int a[2] = { 61810, 2025 };
    memdump("ii", (char*) a);
    
    printf("Example 2:\n");
    memdump("S", "a string");
    
    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *) &s);

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;
    
    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");
    
    printf("Example 4:\n");
    memdump("pihcS", (char*) &example);
    
    printf("Example 5:\n");
    memdump("sccccc", (char*) &example);
  } else if(argc == 2){
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while(n < sizeof(data)){
      int nn = read(0, data + n, sizeof(data) - n);
      if(nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void
memdump(char *fmt, char *data)
{
  // Your code here.
  char *p = data;  // 当前读取位置
  for(int i = 0; fmt[i] != '\0'; i++){
    switch(fmt[i]){
      case 'i':{
        //即int
        int val = *(int *)p;
        printf("%d\n", val);
        p += 4;
        break; 
      }

      case 'p': {
        unsigned long val = *(unsigned long *)p;
        printf("%lx\n", val);
        p += 8;
        break;
      }

      case 'h': {
        short val = *(short *)p;
        printf("%d\n", val);
        p += 2;
        break;
      }
            
      case 'c': {
        char val = *p;
        printf("%c\n", val);
        p += 1;
        break;
      }
            
      case 's': {
        char *str_ptr = *(char **)p;
        printf("%s\n", str_ptr);
        p += 8;
        break;
      }
            
      case 'S': {
        // 剩余所有数据作为字符串
        printf("%s\n", p);
        while (*p != '\0') p++;
        p++;
        break;
      }
            
      default:
        break;
    }
  }
}
