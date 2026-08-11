#include <syscalls/syscalls.h>

long sys_getegid(void){
    task_t* self = task_scheduler::get_current_task();
    return self->egid;
}
REGISTER_SYSCALL(SYS_getegid, sys_getegid);
