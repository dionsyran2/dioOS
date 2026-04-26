#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>
#include <syscalls/files/files.h>

long sys_open(const char* ustring, int flags, int mode){
    task_t* self = task_scheduler::get_current_task();
    char path[2048];
    fix_path(ustring, path, sizeof(path), self);

    vnode_t* node = vfs::resolve_path(path);
    
    if (node && (flags & O_CREAT) == O_CREAT && (flags & O_EXCL) == O_EXCL) return -EEXIST;

    if ((flags & O_CREAT) == O_CREAT && !node){
        uint64_t ret = sys_creat(ustring, mode);
        return ret;
    }

    if (!node) return -ENOENT;

    if (node->file_operations.special_open) {
        int ret = node->file_operations.special_open(node);
        node->close();
        return ret;
    }
    
    uint8_t required_perms = 0;
    if ((flags & 0b11) == O_WRONLY) required_perms |= 02;
    if ((flags & 0b11) == O_RDONLY) required_perms |= 04;
    if ((flags & 0b11) == O_RDWR) required_perms |= 02 | 04;

    int r = vfs::vfs_check_permission(node, required_perms);
    if (r != 0) {
        node->close();
        return -EACCES;
    }

    fd_t* fd = self->open_node(node);
    fd->flags = flags;
    return fd->num;
}
REGISTER_SYSCALL(SYS_open, sys_open);


uint64_t sys_openat(int dirfd, const char* ustring, int flags, int mode){
    task_t* self = task_scheduler::get_current_task();
    char path[2048];
    fix_path(dirfd, ustring, path, sizeof(path), self);

    vnode_t* node = vfs::resolve_path(path);
    
    if (node && (flags & O_CREAT) == O_CREAT && (flags & O_EXCL) == O_EXCL) return -EEXIST;

    if ((flags & O_CREAT) == O_CREAT && !node){
        uint64_t ret = sys_creat(ustring, mode);
        return ret;
    }

    if (!node) return -ENOENT;

    if (node->file_operations.special_open) {
        int ret = node->file_operations.special_open(node);
        node->close();
        return ret;
    }
    
    uint8_t required_perms = 0;
    if ((flags & 0b11) == O_WRONLY) required_perms |= 02;
    if ((flags & 0b11) == O_RDONLY) required_perms |= 04;
    if ((flags & 0b11) == O_RDWR) required_perms |= 02 | 04;

    int r = vfs::vfs_check_permission(node, required_perms);
    if (r == -EACCES) {
        node->close();
        return -EACCES;
    }

    fd_t* fd = self->open_node(node);
    fd->flags = flags;
    return fd->num;
}
REGISTER_SYSCALL(SYS_openat, sys_openat);