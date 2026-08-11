#include <syscalls/syscalls.h>
#include <bits/fcntl.h>

int64_t access(const char *path, int mode){
    task_t *self = task_scheduler::get_current_task();

    char *kpath = self->read_string(path);
    if (!kpath) return -EFAULT;

    int intent = 0;
    if (mode == F_OK){
        intent = 0;
    } else {
        if (mode & R_OK) intent |= MAY_READ;
        if (mode & W_OK) intent |= MAY_WRITE;
        if (mode & X_OK) intent |= MAY_EXEC;
    }
    
    intent |= (1 << 31); // Tell it to use ruid/rgid

    int errno = 0;
    vnode_t *node = vfs::resolve_path(kpath, intent, errno);
    if (node){
        node->close();
    }

    free(kpath);
    return errno;
}

REGISTER_SYSCALL(SYS_access, access);