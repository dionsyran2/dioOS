#include <syscalls/syscalls.h>

long sys_dup(int oldfd){
    task_t* self = task_scheduler::get_current_task();

    file_t* file = self->fd_table->get_file(oldfd);

    if (file == nullptr) {
        return -EBADF;
    }

    return self->fd_table->dup(file);
}

REGISTER_SYSCALL(SYS_dup, sys_dup);