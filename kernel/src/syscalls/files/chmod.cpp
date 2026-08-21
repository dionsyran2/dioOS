#include <syscalls/syscalls.h>

long chmod(char *upath, mode_t mode){
    task_t* self = task_scheduler::get_current_task();

    char *path = self->read_string(upath);
    vnode_t *node = vfs::resolve_path(path);
    free(path);

    if (!node) return -ENOENT;

    // Check if we have permission
    if (self->euid != node->attributes.uid && self->euid != 0) {
        node->close();
        return -EPERM;
    }

    vnode_attributes_t attr = {
        .valid = VNODE_ATTR_MODE,
        .mode = (node->attributes.mode & S_IFMT) | mode,
    };

    // Set the mode
    node->set_attributes(&attr);

    node->close();

    return 0;
}

REGISTER_SYSCALL(SYS_chmod, chmod);

int fchmod(int fd, mode_t mode){
    task_t* self = task_scheduler::get_current_task();

    file_t *file = self->fd_table->get_file(fd);

    if (!file) return -EBADFD;

   // Check if we have permission
    if (self->euid != file->node->attributes.uid && self->euid != 0) {
        return -EPERM;
    }

    vnode_attributes_t attr = {
        .valid = VNODE_ATTR_MODE,
        .mode = (file->node->attributes.mode & S_IFMT) | mode,
    };

    // Set the mode
    file->node->set_attributes(&attr);
    return 0;
}

REGISTER_SYSCALL(SYS_fchmod, fchmod);