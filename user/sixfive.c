#include "kernel/types.h"
#include "user/user.h"

//是分隔符返回1，不是返回0
int is_separator(char c)
{
    char separators[]="-\r\t\n./,";
    return strchr(separators, c) != 0;
}

int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

// 判断是否是5或6的倍数
int is_multiple_of_5or6(int num)
{
    return (num % 5 == 0) || (num % 6 == 0);
}

//处理文件
void process_file(char *filename)
{
    int fd;//检查是否打开
    char c;
    int num = 0, bytes_read, has_digit = 0;

    fd = open(filename, 0);  // 0 = 只读模式
    if (fd < 0) {
        fprintf(2, "sixfive: cannot open %s\n", filename);
        return;
    }

    while((bytes_read = read(fd, &c, 1)) > 0){
        if(is_digit(c)){
            num = num * 10 + c - '0';
            has_digit = 1;
        }
        else if(is_separator(c)){
            if(has_digit && is_multiple_of_5or6(num)){
                printf("%d\n", num);
            }
            num = 0;
            has_digit = 0;
        }
    }

    if (has_digit && is_multiple_of_5or6(num)) {
        printf("%d\n", num);
    }

    close(fd);
}

int main(int arg_count, char *arg_vector[])
{
    if (arg_count < 2) {
        fprintf(2, "Usage: sixfive <files...>\n");
        exit(1);
    }

    for (int i = 1; i < arg_count; i++) {
        process_file(arg_vector[i]);
    }

    exit(0);
}