#include <syscalls/syscalls.h>


long sys_getgid(void){
    task_t* ctask = task_scheduler::get_current_task();
    return ctask->rgid;
}
REGISTER_SYSCALL(SYS_getgid, sys_getgid);

long sys_getegid(void){
    task_t* ctask = task_scheduler::get_current_task();
    return ctask->egid;
}
REGISTER_SYSCALL(SYS_getegid, sys_getegid);


long sys_setgid(int gid){
    task_t* self = task_scheduler::get_current_task();
    
    if (self->egid != 0 && gid != self->rgid && gid != self->egid && gid != self->sgid)
        return -EPERM;


    self->egid = gid;
    self->rgid = gid;
    return 0;
}
REGISTER_SYSCALL(SYS_setgid, sys_setgid);



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

long sys_getresgid(int* rgid, int* egid, int* sgid){
    task_t* self = task_scheduler::get_current_task();
    self->write_memory(rgid, &self->rgid, sizeof(int));
    self->write_memory(egid, &self->egid, sizeof(int));
    self->write_memory(sgid, &self->sgid, sizeof(int));

    return 0;
}
REGISTER_SYSCALL(SYS_getresgid, sys_getresgid);

long sys_setregid(int rgid, int egid){
    task_t* self = task_scheduler::get_current_task();
    self->ruid = rgid;
    self->euid = egid;
    return 0;
}
REGISTER_SYSCALL(SYS_setregid, sys_setregid);

long setgroups(size_t size, gid_t list[]){
    return 0;
}
REGISTER_SYSCALL(SYS_setgroups, setgroups);