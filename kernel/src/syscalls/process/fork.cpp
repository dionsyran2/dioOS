#include <syscalls/syscalls.h>
#include <memory.h>

long sys_fork(){
    task_t* self = task_scheduler::get_current_task();
    task_t* child = task_scheduler::fork_process(self);

    task_scheduler::mark_ready(child);
    
    return child->pid;
}

REGISTER_SYSCALL(SYS_fork, sys_fork);
REGISTER_SYSCALL(SYS_clone, sys_fork);


long sys_vfork(){
    task_t* self = task_scheduler::get_current_task();
    task_t* child = task_scheduler::fork_process(self, true, true);
    int r = child->pid;

    task_scheduler::mark_ready(child);

    self->Block(WAIT_FOR_CHILD, r);

    return r;
}

REGISTER_SYSCALL(SYS_vfork, sys_vfork);