#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>

int link(){
    return 0;
}

REGISTER_SYSCALL(86, link);