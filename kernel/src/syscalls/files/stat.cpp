#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>
#include <syscalls/files/files.h>
#include <time.h>


struct stat {
	unsigned long st_dev;
	unsigned long st_ino;
	unsigned long st_nlink;

	unsigned int st_mode;
	unsigned int st_uid;
	unsigned int st_gid;
	int __pad0;
	unsigned long st_rdev;
	long st_size;
	long st_blksize;
	long st_blocks;

	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
	long __unused[3];
} __attribute__((packed));





long sys_stat_internal(vnode_t* node, struct stat* statbuf, bool lnk){
    if (node == nullptr) return -ENOENT;

    task_t* self = task_scheduler::get_current_task();

    statbuf->st_dev = node->inode;
    statbuf->st_ino = node->inode;
    statbuf->st_mode =  node->permissions;
    
    switch(node->type){
        case VDIR:
            statbuf->st_mode |= S_IFDIR;
            break;
        case VCHR :
            statbuf->st_mode |= S_IFCHR;
            break;
        case VBLK :
            statbuf->st_mode |= S_IFBLK;
            break;
        case VREG :
            statbuf->st_mode |= S_IFREG;
            break;
        case VLNK :
            statbuf->st_mode |= S_IFLNK;
            break;
        case VSOC :
            statbuf->st_mode |= S_IFSOCK;
            break;
        case VPIPE :
            statbuf->st_mode |= S_IFIFO;
            break;
    }

    statbuf->st_nlink = 1;
    statbuf->st_uid = node->uid;
    statbuf->st_gid = node->gid;
    statbuf->st_rdev = node->dev_id;
    statbuf->st_size = node->size;

    // Size and blocks for regular files and block devices
    
    statbuf->st_size = node->size;
    statbuf->st_blocks = (node->size + 511) / 512; // approximate block count
    statbuf->st_blksize = node->io_block_size;


    statbuf->st_atim.tv_nsec = 0;
    statbuf->st_atim.tv_sec = node->last_accessed;
    statbuf->st_mtim.tv_nsec = 0;
    statbuf->st_mtim.tv_sec = node->last_modified;
    statbuf->st_ctim.tv_nsec = 0;
    statbuf->st_ctim.tv_sec = node->creation_time;

    
    return 0;
};

long sys_stat(char* fn, struct stat* out){
    task_t* self = task_scheduler::get_current_task();

    struct stat statbuf;

    char pathname[512] = { };
    fix_path(fn, pathname, sizeof(pathname), self); 


    vnode_t* node = vfs::resolve_path(pathname, true);

    long ret = sys_stat_internal(node, &statbuf, true);

    self->write_memory(out, &statbuf, sizeof(stat));

    return ret;
}

REGISTER_SYSCALL(SYS_stat, sys_stat);

long sys_lstat(char* fn, struct stat* out){
    task_t* self = task_scheduler::get_current_task();

    struct stat statbuf;

    char pathname[512] = { };
    fix_path(fn, pathname, sizeof(pathname), self); 


    vnode_t* node = vfs::resolve_path(pathname, true, false);

    long ret = sys_stat_internal(node, &statbuf, false);

    self->write_memory(out, &statbuf, sizeof(stat));

    return ret;
}

REGISTER_SYSCALL(SYS_lstat, sys_lstat);

long sys_fstat(int fd, struct stat* out){
    task_t* self = task_scheduler::get_current_task();
    fd_t* ofd = self->get_fd(fd);

    if (ofd == nullptr) return -EBADF;
    struct stat statbuf;

    long ret = sys_stat_internal(ofd->node, &statbuf, true);

    self->write_memory(out, &statbuf, sizeof(stat));

    return ret;
}

REGISTER_SYSCALL(SYS_fstat, sys_fstat);



long sys_newfstatat(int dirfd, const char* fn, stat* out, int flags){
    task_t* self = task_scheduler::get_current_task(); 

    char pathname[512] = { };
    fix_path(dirfd, fn, pathname, sizeof(pathname), self);
    vnode_t* node = vfs::resolve_path(pathname, true, (flags & AT_SYMLINK_NOFOLLOW) == 0);

    struct stat statbuf;

    long ret = sys_stat_internal(node, &statbuf, (flags & AT_SYMLINK_NOFOLLOW) == 0);

    self->write_memory(out, &statbuf, sizeof(stat));
    return ret;
}

REGISTER_SYSCALL(SYS_newfstatat, sys_newfstatat);
