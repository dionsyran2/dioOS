#include <syscalls/syscalls.h>


int chown(const char *upath, uid_t owner, gid_t group){
    task_t* self = task_scheduler::get_current_task();
    char *path = self->read_string(upath);
    vnode_t *node = vfs::resolve_path(path);
    free(path);

    if (!node) return -ENOENT;

    if (self->euid != 0 && self->euid != node->attributes.uid) {
        node->close();
        return -EPERM;
    }

    vnode_attributes_t attr = { 0 };
    // Set the mode
    if (owner != (uid_t)-1) {
        attr.valid |= VNODE_ATTR_UID;
        attr.uid = owner;
    }
    if (owner != (uid_t)-1) {
        attr.valid |= VNODE_ATTR_GID;
        attr.gid = group;
    }
    
    int r = node->set_attributes(&attr);

    node->close();
    return r;
}

REGISTER_SYSCALL(SYS_chown, chown);

int fchown(int fd, uid_t owner, gid_t group){
    task_t* self = task_scheduler::get_current_task();

    file_t *file = self->fd_table->get_file(fd);

    if (!file) return -EBADF;

    if (self->euid != 0 && self->euid != file->node->attributes.uid) {
        file->node->close();
        return -EPERM;
    }

    vnode_attributes_t attr = { 0 };
    // Set the mode
    if (owner != (uid_t)-1) {
        attr.valid |= VNODE_ATTR_UID;
        attr.uid = owner;
    }
    if (owner != (uid_t)-1) {
        attr.valid |= VNODE_ATTR_GID;
        attr.gid = group;
    }
    
    int r = file->node->set_attributes(&attr);

    return r;
}
REGISTER_SYSCALL(SYS_fchown, fchown);


int lchown(const char *upath, uid_t owner, gid_t group){
    task_t* self = task_scheduler::get_current_task();
    char *path = self->read_string(upath);
    vnode_t *node = vfs::resolve_path(path);
    free(path);

    if (!node) return -ENOENT;

    if (self->euid != 0 && self->euid != node->attributes.uid) {
        node->close();
        return -EPERM;
    }

    vnode_attributes_t attr = { 0 };
    // Set the mode
    if (owner != (uid_t)-1) {
        attr.valid |= VNODE_ATTR_UID;
        attr.uid = owner;
    }
    if (owner != (uid_t)-1) {
        attr.valid |= VNODE_ATTR_GID;
        attr.gid = group;
    }
    
    int r = node->set_attributes(&attr);

    node->close();
    return r;
}
REGISTER_SYSCALL(SYS_lchown, lchown);