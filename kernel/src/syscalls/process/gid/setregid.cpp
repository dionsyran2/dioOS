#include <syscalls/syscalls.h>

long sys_setregid(int rgid, int egid){
    task_t* self = task_scheduler::get_current_task();
    self->ruid = rgid;
    self->euid = egid;
    return 0;
}
REGISTER_SYSCALL(SYS_setregid, sys_setregid);