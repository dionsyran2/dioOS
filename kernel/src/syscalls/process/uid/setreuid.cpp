#include <syscalls/syscalls.h>

long sys_setreuid(int ruid, int euid){
    task_t* self = task_scheduler::get_current_task();

    if (self->euid != 0 && ruid != self->ruid && ruid != self->euid && ruid != self->suid)
        return -EPERM;
        
    if (self->euid != 0 && euid != self->ruid && euid != self->euid && euid != self->suid)
        return -EPERM;


    self->ruid = ruid;
    self->euid = euid;
    return 0;
}

REGISTER_SYSCALL(SYS_setreuid, sys_setreuid);