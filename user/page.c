#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    sleep(1);
    getLivePage();
    return 0;
}