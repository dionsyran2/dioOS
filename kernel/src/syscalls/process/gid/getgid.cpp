#include <syscalls/syscalls.h>

int getgid(){
    task_t *self = task_scheduler::get_current_task();

    return self->rgid;
}

REGISTER_SYSCALL(SYS_getgid, getgid);