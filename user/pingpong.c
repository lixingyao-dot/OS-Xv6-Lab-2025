#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int fd1[2];
    int fd2[2];
    pipe(fd1);
    pipe(fd2);
    char buff[20];
    if(fork()==0){
    	read(fd1[0],buff,4);
    	printf("%d: received %s\n",getpid(),buff);
    	write(fd2[1],"pong",strlen("pong"));
    }
    else {
	write(fd1[1],"ping", strlen("ping"));
	wait(0); 
	read(fd2[0],buff,4);
	printf("%d: received %s\n",getpid(),buff);
    }
    exit(0);
}
