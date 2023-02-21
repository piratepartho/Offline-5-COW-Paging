#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/memlayout.h"

int main()
{
    int a=2;
    a = a*2;
    // char *p = sbrk((PHYSTOP - KERNBASE) / 3);
    // *(int*) p = 1234;
    int pid = fork();
    getPageInfo();
    sleep(1);
    if(pid == 0){
        // sbrk((PHYSTOP - KERNBASE) / 3);
        char *p = sbrk(4096);
        * (int*) p = 1234;
        // for(char* q = p; q < p + (PHYSTOP - KERNBASE) / 3; q+=4096){
        //     *(int*) q = 1234;
        // }
    }
    wait(0);
    getPageInfo();
    sleep(1);
    return 0;
}