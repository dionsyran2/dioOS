#include <syscalls/syscalls.h>

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

int sigprocmask(int how, const kernel_sigset_t *set, kernel_sigset_t *oldset, size_t sigsetsize){
    if (sigsetsize != sizeof(kernel_sigset_t)) return -EINVAL;

    task_t *self = task_scheduler::get_current_task();

    // Copy the old set
    if (oldset){
        if (!self->write_memory(oldset, &self->signal_mask, sizeof(kernel_sigset_t)))
            return -EFAULT;
    }

    if (set){
        // Read the new set
        kernel_sigset_t newset;
        if (!self->read_memory(set, &newset, sizeof(kernel_sigset_t)))
            return -EFAULT;

        // Apply it
        switch (how){
            case SIG_BLOCK:
                self->signal_mask |= newset;
                break;
            case SIG_UNBLOCK:
                self->signal_mask &= ~newset;
                break;
            case SIG_SETMASK:
                self->signal_mask = newset;
                break;
            default:
                return -EINVAL;
        }
    }

    return 0;
}

REGISTER_SYSCALL(SYS_sigprocmask, sigprocmask);