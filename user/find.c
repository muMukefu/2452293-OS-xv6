#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

// 全局变量：保存 exec 命令
char* exec_cmd = 0;
char* exec_args[MAXARG];

// 递归查找函数
void find(char* path, char* target)
{
  int fd;
  struct dirent de;
  struct stat st;
  char buf[512], * p;

  //打开目录
  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  //目录状态
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  //如果不是目录，直接比较文件名
  if (st.type != T_DIR) {
    //提取文件名
    char* last_slash = 0;
    for (char* q = path; *q; q++) {
      if (*q == '/') {
        last_slash = q;
      }
    }
    char* filename = (last_slash ? last_slash + 1 : path);

    if (strcmp(filename, target) == 0) {
      //有-exec命令
      if (exec_cmd != 0) {
        int pid = fork();
        if (pid < 0) {
          fprintf(2, "find: fork failed\n");
        }
        else if (pid == 0) {
          //构建参数——命令+文件路径
          char* argv[MAXARG];
          int i = 0;
          argv[i++] = exec_cmd;
          for (int j = 0; exec_args[j] != 0; j++) {
            argv[i++] = exec_args[j];
          }
          argv[i++] = path;
          argv[i] = 0;

          exec(exec_cmd, argv);
          //exec失败
          fprintf(2, "find: exec failed\n");
          exit(1);
        }
        else {
          //等待子进程结束
          wait(0);
        }
      }
      else {
        //没有-exec，原find实现
        printf("%s\n", path);
      }
    }
    close(fd);
    return;
  }

  //是目录，遍历所有条目
  strcpy(buf, path);
  p = buf + strlen(buf);
  if (p > buf && *(p - 1) != '/') {
    *p++ = '/';
  }

  //读取目录内容
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) {
      continue;
    }

    //跳过.和..
    if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
      continue;
    }

    //完整路径
    char* q = p;
    for (char* s = de.name; *s; s++) {
      *q++ = *s;
    }
    *q = '\0';

    //获取子文件状态
    struct stat child_st;
    if (stat(buf, &child_st) < 0) {
      continue;
    }

    //目录——递归查找
    if (child_st.type == T_DIR) {
      find(buf, target);
    }
    else {
      //文件——检查文件名是否匹配
      if (strcmp(de.name, target) == 0) {
        if (exec_cmd != 0) {
          int pid = fork();
          if (pid < 0) {
            fprintf(2, "find: fork failed\n");
          }
          else if (pid == 0) {
            char* argv[MAXARG];
            int i = 0;
            argv[i++] = exec_cmd;
            for (int j = 0; exec_args[j] != 0; j++) {
              argv[i++] = exec_args[j];
            }
            argv[i++] = buf;
            argv[i] = 0;
            exec(exec_cmd, argv);
            fprintf(2, "find: exec failed\n");
            exit(1);
          }
          else {
            wait(0);
          }
        }
        else {
          printf("%s\n", buf);
        }
      }
    }
  }

  close(fd);
}

int main(int arg_count, char* arg_vector[])
{
  if (arg_count < 3) {
    fprintf(2, "Usage: find <directory> <filename>\n");
    exit(1);
  }

  //检查有无-exec参数
  int exec_pos = -1;
  for (int i = 3; i < arg_count; i++) {
    if (strcmp(arg_vector[i], "-exec") == 0) {
      exec_pos = i;
      break;
    }
  }

  if (exec_pos != -1) {
    //解析命令和参数
    exec_cmd = arg_vector[exec_pos + 1];
    int arg_idx = 0;
    for (int i = exec_pos + 2; i < arg_count; i++) {
      exec_args[arg_idx++] = arg_vector[i];
    }
    exec_args[arg_idx] = 0;
  }

  find(arg_vector[1], arg_vector[2]);
  exit(0);
}
