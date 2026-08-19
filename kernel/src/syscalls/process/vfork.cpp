#include <syscalls/syscalls.h>

long sys_vfork(){
    task_t* self = task_scheduler::get_current_task();
    task_t* child = task_scheduler::clone(CLONE_VM | CLONE_FS, 0, self->syscall_registers);
    task_scheduler::mark_as_ready(child);

    int r = child->pid;

    while (1) {
            self->block();


            if (self->block_status == -EINTR && self->block_intr_info != SIGCHLD) return -EINTR;
            if (child->current_state != ZOMBIE) continue;
            break;
        }
    
    return r;
}

REGISTER_SYSCALL(SYS_vfork, sys_vfork);