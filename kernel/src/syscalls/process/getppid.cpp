#include <syscalls/syscalls.h>

int getppid(){
    task_t *self = task_scheduler::get_current_task();

    return self->ppid;
}

REGISTER_SYSCALL(SYS_getppid, getppid);