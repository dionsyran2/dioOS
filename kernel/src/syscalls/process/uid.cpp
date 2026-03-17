#include <syscalls/syscalls.h>


long sys_getuid(void){
    task_t* self = task_scheduler::get_current_task();
    return self->ruid;
}

REGISTER_SYSCALL(SYS_getuid, sys_getuid);

long sys_geteuid(void){
    task_t* self = task_scheduler::get_current_task();

    return self->euid;
}
REGISTER_SYSCALL(SYS_geteuid, sys_geteuid);


long sys_setuid(int uid){
    task_t* self = task_scheduler::get_current_task();
    if (self->euid != 0 && uid != self->ruid && uid != self->euid && uid != self->suid)
        return -EPERM;

    self->euid = uid;
    self->ruid = uid;
    return 0;
}
REGISTER_SYSCALL(SYS_setuid, sys_setuid);


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

long sys_getresuid(int* ruid, int* euid, int* suid){
    task_t* self = task_scheduler::get_current_task();
    self->write_memory(ruid, &self->ruid, sizeof(int));
    self->write_memory(euid, &self->euid, sizeof(int));
    self->write_memory(suid, &self->suid, sizeof(int));
    return 0;
}
REGISTER_SYSCALL(SYS_getresuid, sys_getresuid);


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

long sys_setsid(){
    task_t* self = task_scheduler::get_current_task();
    self->sid = self->pid;
    return self->sid;
}

REGISTER_SYSCALL(SYS_setsid, sys_setsid);