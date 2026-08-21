#include <syscalls/syscalls.h>
#include <bits/stat.h>

long sys_stat_internal(vnode_t* node, struct stat* statbuf){
    if (node == nullptr) return -ENOENT;

    task_t* self = task_scheduler::get_current_task();

    statbuf->st_dev = node->fs_id;
    statbuf->st_ino = node->inode ? node->inode : 0;
    statbuf->st_mode =  node->attributes.mode;

    statbuf->st_nlink = node->nlink;
    statbuf->st_uid = node->attributes.uid;
    statbuf->st_gid = node->attributes.gid;
    statbuf->st_rdev = 0;
    statbuf->st_size = node->size;

    // Size and blocks for regular files and block devices
    
    statbuf->st_size = node->size;
    statbuf->st_blocks = (node->size + 511) / 512; // approximate block count
    statbuf->st_blksize = 512;


    statbuf->st_atim.tv_nsec = 0;
    statbuf->st_atim.tv_sec = node->attributes.atime;
    statbuf->st_mtim.tv_nsec = 0;
    statbuf->st_mtim.tv_sec = node->attributes.mtime;
    statbuf->st_ctim.tv_nsec = 0;
    statbuf->st_ctim.tv_sec = node->attributes.ctime;

    
    return 0;
};

long sys_stat(const char* fn, struct stat* out){
    task_t* self = task_scheduler::get_current_task();

    char* kpath = self->read_string((char*)fn);
    if (!kpath) return -EFAULT;

    int err = 0;
    vnode_t* node = vfs::resolve_path(kpath, 0, err);
    free(kpath);

    if (!node) return err;

    struct stat statbuf;
    long ret = sys_stat_internal(node, &statbuf);

    if (ret == 0) {
        if (self->write_to_userspace(out, &statbuf, sizeof(stat)) < 0) {
            ret = -EFAULT;
        }
    }

    node->close();
    return ret;
}

REGISTER_SYSCALL(SYS_stat, sys_stat);

long sys_lstat(const char* fn, struct stat* out){
    task_t* self = task_scheduler::get_current_task();

    char* kpath = self->read_string((char*)fn);
    if (!kpath) return -EFAULT;

    int err = 0;
    vnode_t* node = vfs::resolve_path(kpath, 0, err, true, false);
    free(kpath);

    if (!node) return err;

    struct stat statbuf;
    long ret = sys_stat_internal(node, &statbuf);

    if (ret == 0) {
        if (self->write_to_userspace(out, &statbuf, sizeof(stat)) < 0) {
            ret = -EFAULT;
        }
    }

    node->close();
    return ret;
}

REGISTER_SYSCALL(SYS_lstat, sys_lstat);

long sys_fstat(int fd, struct stat* out){
    task_t* self = task_scheduler::get_current_task();
    file_t *file = self->fd_table->get_file(fd);

    if (file == nullptr) return -EBADF;
    struct stat statbuf;

    long ret = sys_stat_internal(file->node, &statbuf);

    self->write_to_userspace(out, &statbuf, sizeof(stat));

    return ret;
}

REGISTER_SYSCALL(SYS_fstat, sys_fstat);


#define AT_SYMLINK_NOFOLLOW 0x100

long sys_newfstatat(int dirfd, const char* fn, struct stat* out, int flags){
    task_t* self = task_scheduler::get_current_task(); 
    
    char *kpath = self->read_string((char*)fn);
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
    bool follow_trailing = !(flags & AT_SYMLINK_NOFOLLOW);
    
    dentry_t *dentry = vfs::resolve_path_dentry_at(base_dir, kpath, 0, err, true, follow_trailing);
    free(kpath);
    
    if (!dentry) return err;

    vnode_t *node = vfs::_get_vnode(dentry);
    if (!node) {
        dentry->unref();
        return -EIO;
    }

    struct stat statbuf;
    long ret = sys_stat_internal(node, &statbuf);
    
    if (ret == 0) {
        self->write_to_userspace(out, &statbuf, sizeof(stat));
    }

    node->close();
    dentry->unref();
    
    return ret;
}

REGISTER_SYSCALL(SYS_newfstatat, sys_newfstatat);