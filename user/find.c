#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 从路径中提取文件名
char*
extract_filename(char *path)
{
    char *p;

    // 从路径末尾开始向前查找最后一个'/'
    for(p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++; // 跳过'/'

    return p;
}

// 比较文件名是否匹配
int
match(char *path, char *filename)
{
    char *p = extract_filename(path);
    return strcmp(p, filename) == 0;
}

// 递归查找文件
void
find(char *path, char *filename)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    // 打开路径
    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // 获取文件状态
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // 根据文件类型处理
    switch(st.type){
    case T_FILE:
        // 如果是文件且名称匹配，则打印路径
        if(match(path, filename)){
            printf("%s\n", path);
        }
        break;

    case T_DIR:
        // 检查路径长度是否超过缓冲区
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
            printf("find: path too long\n");
            break;
        }
        
        // 构造新路径
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/'; // 添加路径分隔符
        
        // 读取目录项
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0)
                continue; // 跳过空目录项
            
            // 复制文件名到路径末尾
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0; // 确保字符串结束
            
            // 跳过"."和".."
            if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            
            // 递归查找子目录
            find(buf, filename);
        }
        break;
    }
    close(fd);
}

int
main(int argc, char *argv[])
{
    // 检查参数数量
    if(argc != 3){
        fprintf(2, "Usage: find <directory> <filename>\n");
        exit(1);
    }

    // 开始查找
    find(argv[1], argv[2]);
    
    exit(0);
}