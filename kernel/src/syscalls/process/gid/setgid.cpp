#include <syscalls/syscalls.h>

long sys_setgid(int gid){
    task_t* self = task_scheduler::get_current_task();
    
    if (self->egid != 0 && gid != self->rgid && gid != self->egid && gid != self->sgid)
        return -EPERM;


    self->egid = gid;
    self->rgid = gid;
    return 0;
}
REGISTER_SYSCALL(SYS_setgid, sys_setgid);
