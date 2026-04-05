#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>


int chown(const char *upath, uid_t owner, gid_t group){
    task_t* self = task_scheduler::get_current_task();
    char path[2048];
    fix_path(upath, path, sizeof(path), self);

    vnode_t *node = vfs::resolve_path(path);

    if (!node) return -ENOENT;

    if (self->euid != 0 && self->euid != node->uid) {
        node->close();
        return -EPERM;
    }

    // Set the mode
    if (owner != (uid_t)-1) node->uid = owner;
    if (group != (gid_t)-1) node->gid = group;
    node->save_metadata();

    node->close();
    return 0;
}

REGISTER_SYSCALL(SYS_chown, chown);

int fchown(int fd, uid_t owner, gid_t group){
    task_t* self = task_scheduler::get_current_task();

    fd_t *ofd = self->get_fd(fd);

    if (!ofd) return -ENOENT;

    if (self->euid != 0 && self->euid != ofd->node->uid)  return -EPERM;
    // Set the mode
    if (owner != (uid_t)-1) ofd->node->uid = owner;
    if (group != (gid_t)-1) ofd->node->gid = group;
    ofd->node->save_metadata();
    return 0;
}
REGISTER_SYSCALL(SYS_fchown, fchown);


int lchown(const char *upath, uid_t owner, gid_t group){
    task_t* self = task_scheduler::get_current_task();
    char path[2048];
    fix_path(upath, path, sizeof(path), self);

    vnode_t *node = vfs::resolve_path(path, true, false);

    if (!node) return -ENOENT;

    if (self->euid != 0 && self->euid != node->uid) {
        node->close();
        return -EPERM;
    }

    // Set the mode
    if (owner != (uid_t)-1) node->uid = owner;
    if (group != (gid_t)-1) node->gid = group;
    node->save_metadata();

    node->close();
    return 0;
}
REGISTER_SYSCALL(SYS_lchown, lchown);
