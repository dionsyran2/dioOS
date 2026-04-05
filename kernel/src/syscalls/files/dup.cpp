#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/files/fcntl.h>



long sys_dup2(int oldfd, int newfd){
    if (oldfd == newfd) return oldfd;
    task_t* self = task_scheduler::get_current_task();

    fd_t* ofd = self->get_fd(oldfd);

    if (ofd == nullptr) return -EBADF;

    fd_t* nfd = self->get_fd(newfd);
    if (nfd != nullptr) sys_close(newfd); // attempt to close the new fd

    ofd->node->open();
    fd_t* fd = self->open_node(ofd->node, newfd);
    fd->flags = ofd->flags & (~O_CLOEXEC);
    fd->node->size = ofd->node->size;
    fd->offset = ofd->offset;
    return fd->num;
}

REGISTER_SYSCALL(SYS_dup2, sys_dup2);

long sys_dup(int oldfd){
    task_t* self = task_scheduler::get_current_task();

    fd_t* ofd = self->get_fd(oldfd);

    if (ofd == nullptr) {
        return -EBADF;
    }

    ofd->node->open();
    fd_t* fd = self->open_node(ofd->node);
    fd->flags = ofd->flags & (~O_CLOEXEC);;
    fd->node->size = ofd->node->size;
    fd->offset = ofd->offset;
    return fd->num;
}

REGISTER_SYSCALL(SYS_dup, sys_dup);
