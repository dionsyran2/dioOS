#include <syscalls/syscalls.h>

long sys_vfork(){
    task_t* self = task_scheduler::get_current_task();
    task_t* child = task_scheduler::clone(CLONE_VM | CLONE_FS, 0, self->syscall_registers);
    task_scheduler::mark_as_ready(child);

    // waitforpid
    self->block();
    return child->pid;
}

REGISTER_SYSCALL(SYS_vfork, sys_vfork);