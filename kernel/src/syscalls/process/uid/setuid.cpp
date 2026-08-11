#include <syscalls/syscalls.h>

long sys_setuid(int uid){
    task_t* self = task_scheduler::get_current_task();
    if (self->euid != 0 && uid != self->ruid && uid != self->euid && uid != self->suid)
        return -EPERM;

    self->euid = uid;
    self->ruid = uid;
    return 0;
}
REGISTER_SYSCALL(SYS_setuid, sys_setuid);