//
// tests for copy-on-write fork() assignment.
//

#include "kernel/types.h"
#include "kernel/memlayout.h"
#include "user/user.h"

// allocate more than half of physical memory,
// then fork. this will fail in the default
// kernel, which does not support copy-on-write.
void
simpletest()
{
  uint64 phys_size = PHYSTOP - KERNBASE;
  int sz = (phys_size / 3) * 2;

  printf("simple: ");
  getPageInfo();
  
  char *p = sbrk(sz);
  if(p == (char*)0xffffffffffffffffL){
    printf("sbrk(%d) failed\n", sz);
    exit(-1);
  }
  printf("allocated\n");
  getPageInfo();

  for(char *q = p; q < p + sz; q += 4096){
    *(int*)q = getpid();
  }
  printf("written\n");
  getPageInfo();

  int pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(-1);
  }
  printf("new fork\n");
  getPageInfo();

  if(pid == 0)
    exit(0);

  wait(0);

  if(sbrk(-sz) == (char*)0xffffffffffffffffL){
    printf("sbrk(-%d) failed\n", sz);
    exit(-1);
  }
  getPageInfo();
  printf("ok\n");
}

int
main(int argc, char *argv[])
{
  simpletest();
  printf("passed\n");
  return 0;
}
