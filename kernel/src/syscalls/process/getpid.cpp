#include <syscalls/syscalls.h>

int getpid(){
    task_t *self = task_scheduler::get_current_task();

    return self->tgid;
}

REGISTER_SYSCALL(SYS_getpid, getpid);