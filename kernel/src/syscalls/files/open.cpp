#include <syscalls/syscalls.h>
#include <bits/fcntl.h>

int64_t sys_openat(int dirfd, const char *pathname, int flags, int mode);

int64_t sys_open(const char *pathname, int flags, mode_t mode){
    return sys_openat(AT_FDCWD, pathname, flags, mode);
}

REGISTER_SYSCALL(SYS_open, sys_open);