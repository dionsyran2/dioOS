#include <syscalls/syscalls.h>
#include <bits/signals.h>


int sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize){
    if (sigsetsize != sizeof(sigset_t)) return -EINVAL;

    task_t *self = task_scheduler::get_current_task();

    sigset_t ksigset;
    // Copy the old set
    if (oldset){
        ksigset.sig[0] = self->blocked_signals;

        if (self->write_to_userspace(oldset, &ksigset, sizeof(sigset_t)))
            return -EFAULT;
    }

    if (set){
        // Read the new set
        if (self->read_from_userspace(&ksigset, (void*)set, sizeof(sigset_t)))
            return -EFAULT;

        // Apply it
        switch (how){
            case SIG_BLOCK:
                self->blocked_signals |= ksigset.sig[0];
                break;
            case SIG_UNBLOCK:
                self->blocked_signals &= ~ksigset.sig[0];
                break;
            case SIG_SETMASK:
                self->blocked_signals = ksigset.sig[0];
                break;
            default:
                return -EINVAL;
        }
    }

    return 0;
}

REGISTER_SYSCALL(SYS_sigprocmask, sigprocmask);