#include <syscalls/syscalls.h>

long sys_setresuid(int ruid, int euid, int suid){
    task_t* self = task_scheduler::get_current_task();

    if (self->euid != 0 && ruid != self->ruid && ruid != self->euid && ruid != self->suid)
        return -EPERM;
        
    if (self->euid != 0 && euid != self->ruid && euid != self->euid && euid != self->suid)
        return -EPERM;

    if (self->euid != 0 && suid != self->ruid && suid != self->euid && suid != self->suid)
        return -EPERM;

    self->ruid = ruid;
    self->euid = euid;
    self->suid = suid;
    return 0;
}

REGISTER_SYSCALL(SYS_setresuid, sys_setresuid);