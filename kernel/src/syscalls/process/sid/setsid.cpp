#include <syscalls/syscalls.h>

long sys_setsid(){
    task_t* self = task_scheduler::get_current_task();
    self->sid = self->pid;
    return self->sid;
}

REGISTER_SYSCALL(SYS_setsid, sys_setsid);