#include <syscalls/syscalls.h>

long sys_fork(){
    task_t* self = task_scheduler::get_current_task();
    task_t* child = task_scheduler::clone(0, 0, self->syscall_registers);
    task_scheduler::mark_as_ready(child);
    
    return child->pid;
}

REGISTER_SYSCALL(SYS_fork, sys_fork);
