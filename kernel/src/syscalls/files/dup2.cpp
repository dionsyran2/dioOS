#include <syscalls/syscalls.h>

long sys_dup2(int oldfd, int newfd){
    if (oldfd == newfd) return oldfd;
    task_t* self = task_scheduler::get_current_task();

    file_t* file = self->fd_table->get_file(oldfd);

    if (file == nullptr) return -EBADF;

    return self->fd_table->dup(file, newfd);
}

REGISTER_SYSCALL(SYS_dup2, sys_dup2);