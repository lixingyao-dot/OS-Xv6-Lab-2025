#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: xargs <command> [args...]\n");
        exit(1);
    }

    // 准备参数数组
    char *args[MAXARG];
    int i;
    
    // 复制原始命令参数
    for (i = 1; i < argc; i++) {
        args[i-1] = argv[i];
    }
    args[i-1] = 0;  // 参数数组必须以0结尾

    // 读取标准输入
    char buf[512];
    char *p = buf;
    int n;
    int pos = 0;
    
    while ((n = read(0, &buf[pos], 1)) > 0) {
        if (buf[pos] == '\n') {
            buf[pos] = '\0';  // 替换换行为字符串结束符
            
            // 添加输入行作为最后一个参数
            args[i-1] = p;
            args[i] = 0;  // 参数数组必须以0结尾
            
            // 创建子进程执行命令
            if (fork() == 0) {
                exec(args[0], args);
                fprintf(2, "xargs: exec %s failed\n", args[0]);
                exit(1);
            }
            wait(0);
            
            // 重置指针和位置
            p = buf;
            pos = 0;
        } else {
            pos++;
            // 检查缓冲区是否溢出
            if (pos >= sizeof(buf) - 1) {
                fprintf(2, "xargs: input line too long\n");
                exit(1);
            }
        }
    }

    // 处理最后一行（如果没有换行符）
    if (pos > 0) {
        buf[pos] = '\0';
        args[i-1] = p;
        args[i] = 0;
        
        if (fork() == 0) {
            exec(args[0], args);
            fprintf(2, "xargs: exec %s failed\n", args[0]);
            exit(1);
        }
        wait(0);
    }

    exit(0);
}