#include <syscalls/syscalls.h>

long sys_setresgid(int rgid, int egid, int sgid){
    task_t* self = task_scheduler::get_current_task();

    if (self->egid != 0 && rgid != self->rgid && rgid != self->egid && rgid != self->sgid)
        return -EPERM;
        
    if (self->egid != 0 && egid != self->rgid && egid != self->egid && egid != self->sgid)
        return -EPERM;

    if (self->egid != 0 && sgid != self->rgid && sgid != self->egid && sgid != self->sgid)
        return -EPERM;

    self->rgid = rgid;
    self->egid = egid;
    self->sgid = sgid;
    return 0;
}
REGISTER_SYSCALL(SYS_setresgid, sys_setresgid);