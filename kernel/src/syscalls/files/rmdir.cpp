#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/fcntl.h>
#include <syscalls/files/files.h>

long rmdir(const char *ustring){
    task_t* self = task_scheduler::get_current_task();
    char path[2048];
    fix_path(ustring, path, sizeof(path), self);

    vnode_t *node = vfs::resolve_path(path);
    if (!node) return -ENOENT;

    int r = node->rmdir();

    node->close();

    return r;
}

REGISTER_SYSCALL(SYS_rmdir, rmdir);