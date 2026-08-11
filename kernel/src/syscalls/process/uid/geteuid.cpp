#include <syscalls/syscalls.h>

long sys_geteuid(void){
    task_t* self = task_scheduler::get_current_task();

    return self->euid;
}
REGISTER_SYSCALL(SYS_geteuid, sys_geteuid);
