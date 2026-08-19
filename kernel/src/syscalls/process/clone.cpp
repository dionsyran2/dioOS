#include <syscalls/syscalls.h>

long sys_clone(unsigned long flags, void *child_stack, int *ptid, unsigned long newtls, int *ctid) {
    task_t* self = task_scheduler::get_current_task();

    task_t* child = task_scheduler::clone(flags, (uint64_t)child_stack, self->syscall_registers);

    if (flags & CLONE_CHILD_SETTID) {
        if (ctid != nullptr) {
            child->write_to_userspace(ctid, &child->pid, sizeof(int));
        }
    }

    if (flags & CLONE_CHILD_CLEARTID) {
        child->clear_child_tid = ctid;
    }

    if (flags & CLONE_PARENT_SETTID) {
        if (ptid != nullptr) {
            self->write_to_userspace(ptid, &child->pid, sizeof(int));
        }
    }

    if (flags & CLONE_SETTLS) {
        child->fs_pointer = newtls;
    }

    task_scheduler::mark_as_ready(child);

    int r = child->pid;

    if (flags & CLONE_VFORK){
        while (1) {
            self->block();


            if (self->block_status == -EINTR && self->block_intr_info != SIGCHLD) return -EINTR;
            if (child->current_state != ZOMBIE) continue;
            break;
        }
    }

    return r;
}
REGISTER_SYSCALL(SYS_clone, sys_clone);