#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>


uint64_t sys_ioctl(unsigned int fd, int op, char* argp){
    task_t* self = task_scheduler::get_current_task();

    fd_t* ofd = self->get_fd(fd);

    if (ofd == nullptr) {
        return -EBADF;
    }
    
    if (ofd->node->type != VCHR) {
        return -ENOTTY;
    }

    int ret = ofd->node->ioctl(op, argp);

    return ret;
}

REGISTER_SYSCALL(SYS_ioctl, sys_ioctl);

