#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int fd[2];
int *get;
int val = 107;
int main()
{
    pipe(fd);

    int pid = fork();

    if(pid == 0){
        write(fd[1], &val, sizeof(int));
        return 0;
    }
    else {
        wait(0);
        read(fd[0],get,sizeof(int));
        if(*get != val){
            printf("parent copyout failed\n");
            exit(-1);
        }
        printf("parent copyout passed\n");
    }

    pid = fork();

    if(pid == 0){
        sleep(1);
        read(fd[0], get, sizeof(int));
        if(*get != val){
            printf("child copyout failed\n");
            exit(-1);
        }
        printf("child copyout passed\n");
        return 0;
    }
    else write(fd[1], &val, sizeof(int));

    
    wait(0);
    printf("cowtest6 passed\n");
    return 0;
}