#include <syscalls/syscalls.h>

long sys_getresgid(int* rgid, int* egid, int* sgid){
    task_t* self = task_scheduler::get_current_task();
    self->write_to_userspace(rgid, &self->rgid, sizeof(int));
    self->write_to_userspace(egid, &self->egid, sizeof(int));
    self->write_to_userspace(sgid, &self->sgid, sizeof(int));

    return 0;
}
REGISTER_SYSCALL(SYS_getresgid, sys_getresgid);