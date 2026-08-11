#include <syscalls/syscalls.h>

int64_t close(int fd){
    task_t *self = task_scheduler::get_current_task();
    return self->fd_table->close_file(fd);
}

REGISTER_SYSCALL(SYS_close, close);