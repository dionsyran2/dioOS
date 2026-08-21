#include <syscalls/syscalls.h>

long sys_getsid(){
    task_t* self = task_scheduler::get_current_task();
    return self->sid;
}

REGISTER_SYSCALL(SYS_getsid, sys_getsid);