#include <syscalls/syscalls.h>

int shutdown(){
    return 0;
}

REGISTER_SYSCALL(SYS_shutdown, shutdown);