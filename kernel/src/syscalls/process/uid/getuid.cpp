#include <syscalls/syscalls.h>

int getuid(){
    task_t *self = task_scheduler::get_current_task();

    return self->ruid;
}

REGISTER_SYSCALL(SYS_getuid, getuid);