#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>


long sys_chown(){
    return 0;
}

REGISTER_SYSCALL(SYS_chown, sys_chown);
REGISTER_SYSCALL(SYS_fchown, sys_chown);