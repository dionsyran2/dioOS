#include <syscalls/syscalls.h>

long sys_getresuid(int* ruid, int* euid, int* suid){
    task_t* self = task_scheduler::get_current_task();
    self->write_to_userspace(ruid, &self->ruid, sizeof(int));
    self->write_to_userspace(euid, &self->euid, sizeof(int));
    self->write_to_userspace(suid, &self->suid, sizeof(int));
    return 0;
}
REGISTER_SYSCALL(SYS_getresuid, sys_getresuid);