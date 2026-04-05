#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>

long chmod(char *upath, mode_t mode){
    task_t* self = task_scheduler::get_current_task();
    char path[2048];
    fix_path(upath, path, sizeof(path), self);

    vnode_t *node = vfs::resolve_path(path);

    if (!node) return -ENOENT;

    // Check if we have permission
    if (self->euid != node->uid && self->euid != 0) {
        node->close();
        return -EPERM;
    }

    // Set the mode
    node->permissions = mode;
    node->save_metadata();

    node->close();

    return 0;
}

REGISTER_SYSCALL(SYS_chmod, chmod);

int fchmod(int fd, mode_t mode){
    task_t* self = task_scheduler::get_current_task();

    fd_t *ofd = self->get_fd(fd);

    if (!ofd) return -ENOENT;

    // Check if we have permission
    if (self->euid != ofd->node->uid && self->euid != 0) return -EPERM;

    // Set the mode
    ofd->node->permissions = mode;
    ofd->node->save_metadata();
    return 0;
}

REGISTER_SYSCALL(SYS_fchmod, fchmod);
