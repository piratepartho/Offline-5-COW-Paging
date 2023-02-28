#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/memlayout.h"
#define PNWRITE 1024
int main()
{
    char* p = sbrk(4096*PNWRITE);

    if(p == (char*)-1){
        printf("sbrk failed\n");
        exit(-1);
    }

    int initPN = getPageInfo();

    int pid = fork();

    if(pid < 0){
        printf("fork failed\n");
        exit(-1);
    }
    

    if(pid == 0){

        int forkPN = getPageInfo();

        if(initPN - forkPN > 10) {
            printf("page cow failed %d\n", initPN - forkPN);
            exit(-1);
        }

        for(char* q = p; q < p+4096*PNWRITE; q+=4096){
            *(int*) q = 1234;
        }
        int afterWritePN = getPageInfo();
        if(forkPN - afterWritePN != PNWRITE) {
            printf("page write not working\n");
            exit(-1);
        }
        printf("page write working\n");
        exit(0);
    }
    wait(0);
    int donePN= getPageInfo();
    if(donePN != initPN){
        printf("page garbage error\n");
        exit(-1);
    }
    printf("no garbage page\n");
    
    printf("Cowtest 5 : Passed\n");

    return 0;
}