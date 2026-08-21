#include <syscalls/syscalls.h>

int chdir(const char *path){
    dentry_t *dentry = vfs::resolve_path_dentry(path);

    if (!dentry) return -ENOENT;

    vnode_t *node = dentry->fetch_vnode();

    if (node) {
        int mode = node->attributes.mode;
        delete node;

        if (!S_ISDIR(mode)) return -ENOTDIR;
    }
    


    task_t *self = task_scheduler::get_current_task();
    
    if (self->fd_table->cwd) self->fd_table->cwd->unref();
    
    self->fd_table->cwd = dentry;
    return 0;
}

REGISTER_SYSCALL(SYS_chdir, chdir);

int fchdir(int fd){
    task_t *self = task_scheduler::get_current_task();
    file_t *file = self->fd_table->get_file(fd);
    if (!file || !file->dentry) return -EBADFD;

    if (!S_ISDIR(file->node->attributes.mode)) return -ENOTDIR;

    
    if (self->fd_table->cwd) self->fd_table->cwd->unref();
    
    file->dentry->ref();
    self->fd_table->cwd = file->dentry;

    return 0;
}
REGISTER_SYSCALL(SYS_fchdir, fchdir);
