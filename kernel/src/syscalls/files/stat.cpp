#include <syscalls/syscalls.h>
#include <bits/stat.h>


long sys_stat_internal(vnode_t* node, struct stat* statbuf, bool lnk){
    if (node == nullptr) return -ENOENT;

    task_t* self = task_scheduler::get_current_task();

    statbuf->st_dev = node->inode;
    statbuf->st_ino = node->inode;
    statbuf->st_mode =  node->attributes.mode;
    

    statbuf->st_nlink = node->nlink;
    statbuf->st_uid = node->attributes.uid;
    statbuf->st_gid = node->attributes.gid;
    statbuf->st_rdev = 1;
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

long sys_stat(char* fn, struct stat* out){
    task_t* self = task_scheduler::get_current_task();

    struct stat statbuf;

    int err;
    vnode_t* node = vfs::resolve_path(fn, 0, err);

    long ret = sys_stat_internal(node, &statbuf, true);

    self->write_to_userspace(out, &statbuf, sizeof(stat));

    node->close();

    return ret;
}

REGISTER_SYSCALL(SYS_stat, sys_stat);

long sys_lstat(char* fn, struct stat* out){
    task_t* self = task_scheduler::get_current_task();

    struct stat statbuf;

    int err;
    vnode_t* node = vfs::resolve_path(fn, 0, err, true, false);

    long ret = sys_stat_internal(node, &statbuf, false);

    self->write_to_userspace(out, &statbuf, sizeof(stat));

    node->close();

    return ret;
}

REGISTER_SYSCALL(SYS_lstat, sys_lstat);

long sys_fstat(int fd, struct stat* out){
    task_t* self = task_scheduler::get_current_task();
    file_t *file = self->fd_table->get_file(fd);

    if (file == nullptr) return -EBADF;
    struct stat statbuf;

    long ret = sys_stat_internal(file->node, &statbuf, true);

    self->write_to_userspace(out, &statbuf, sizeof(stat));

    return ret;
}

REGISTER_SYSCALL(SYS_fstat, sys_fstat);


#define AT_SYMLINK_NOFOLLOW 0x100

long sys_newfstatat(int dirfd, const char* fn, stat* out, int flags){
    task_t* self = task_scheduler::get_current_task(); 

    int fd = self->fd_table->open_file(dirfd, fn, 0, 0);

    if (fd < 0) return fd;
    file_t *file = self->fd_table->get_file(fd);

    struct stat statbuf;

    long ret = sys_stat_internal(file->node, &statbuf, (flags & AT_SYMLINK_NOFOLLOW) == 0);

    self->write_to_userspace(out, &statbuf, sizeof(stat));

    self->fd_table->close_file(fd);
    
    return ret;
}

REGISTER_SYSCALL(SYS_newfstatat, sys_newfstatat);