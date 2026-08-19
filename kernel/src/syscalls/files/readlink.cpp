#include <syscalls/syscalls.h>

long readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz) {
    task_t* self = task_scheduler::get_current_task(); 
    
    char *kpath = self->read_string(path);
    if (!kpath) return -EFAULT;

    dentry_t *base_dir = nullptr;
    if (kpath[0] != '/') {
        if (dirfd == AT_FDCWD) {
            base_dir = self->fd_table->cwd;
        } else {
            file_t *dir_file = self->fd_table->get_file(dirfd);
            if (!dir_file || !dir_file->dentry) {
                free(kpath);
                return -EBADF;
            }
            base_dir = dir_file->dentry;
        }
    }

    int err = 0;
    
    dentry_t *dentry = vfs::resolve_path_dentry_at(base_dir, kpath, 0, err, true, false);
    free(kpath);
    
    if (!dentry) return err;

    vnode_t *node = vfs::_get_vnode(dentry);
    if (!node) {
        dentry->unref();
        return -EIO;
    }

    size_t size = min(bufsiz, 1024);
    char *linkbuf = (char*)malloc(size);
    node->read(linkbuf, size, 0);

    node->close();
    dentry->unref();
    
    int r = self->write_to_userspace(buf, linkbuf, size);

    free(linkbuf);
    return r < 0 ? r : size;
}

REGISTER_SYSCALL(SYS_readlinkat, readlinkat);

long readlink(const char *path, char *buf, size_t bufsiz) {
    return readlinkat(-100, path, buf, bufsiz);
}


REGISTER_SYSCALL(SYS_readlink, readlink);