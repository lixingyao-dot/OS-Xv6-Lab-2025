#include "kernel/types.h" // 包含内核定义的基本数据类型
#include "kernel/stat.h" // 包含文件状态相关的定义
#include "user/user.h" // 包含用户态程序所需的各种系统调用和函数原型 int
// main 函数，程序的入口点，argc 表示命令行参数的数量，argv 是一个指向字符串数组的指针
int main(int argc, char *argv[])
{
// 检查传递给程序的参数数量是否少于 2。如果少于 2，说明用户没有提供需要的时间参数
if(argc < 2){
fprintf(2,"Usage:sleep [time]\n");// 向标准错误输出（文件描述符 2）打印一条使用说明信息
exit(1);// 状态码 1 表示错误
}
int time = atoi(argv[1]);// 将传递的第一个参数（argv[1]）转换为整数，并存储在变量 time 中
sleep(time);// 调用 sleep 函数，使程序暂停执行 time 秒
exit(0);// 状态码 0 表示正确
}
