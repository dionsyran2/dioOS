#include <syscalls/syscalls.h>
#include <bits/fcntl.h>


int64_t sys_openat(int dirfd, const char *pathname, int flags, int mode) {
    task_t *self = task_scheduler::get_current_task();
    if (!self || !self->fd_table) return -EBADF;

    char *kpath = self->read_string(pathname);
    if (!kpath) return -EFAULT;

    int fd = self->fd_table->open_file(dirfd, kpath, flags, mode);
    
    free(kpath);
    return fd;
}
REGISTER_SYSCALL(SYS_openat, sys_openat);
