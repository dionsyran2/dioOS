#include <syscalls/syscalls.h>

long capget(){
    return 0;
}

REGISTER_SYSCALL(125, capget);
REGISTER_SYSCALL(126, capget); // capset