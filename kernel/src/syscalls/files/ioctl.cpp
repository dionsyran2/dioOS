#include <syscalls/syscalls.h>

int ioctl(int fd, uint32_t op, char *arg){
    task_t *self = task_scheduler::get_current_task();

    file_t *file = self->fd_table->get_file(fd);
    if (!file) return -EBADF;

    return file->node->ioctl(op, arg);
}

REGISTER_SYSCALL(SYS_ioctl, ioctl);