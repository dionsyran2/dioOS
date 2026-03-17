#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>

long chmod_stub(){
    return 0;
}

REGISTER_SYSCALL(SYS_fchmod, chmod_stub);