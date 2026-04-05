#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>

long sys_access(const char *ustring, int mode){
    task_t* self = task_scheduler::get_current_task();
    char path[512];
    fix_path(ustring, path, sizeof(path), self);

    vnode_t* node = vfs::resolve_path(path);
    if (node == nullptr){
        return -ENOENT;
    }

    if (mode == 0) return 0;

    // Root check (Root can do anything)
    if (self->ruid == 0) return 0;

    // Owner check
    if (self->ruid == node->uid) {
        return (node->permissions & (mode << 6)) ? 0 : -1;
    }

    // Group check
    if (self->rgid == node->gid) { // Also check supplementary groups list here!
        return (node->permissions & (mode << 3)) ? 0 : -1;
    }

    // Other check
    return (node->permissions & mode) ? 0 : -EACCES;
}

REGISTER_SYSCALL(SYS_access, sys_access);