#include <syscalls/syscalls.h>
#include <bits/signals.h>

int rt_sigaction(int signum, const sigaction *act, sigaction *oldact){
    task_t *self = task_scheduler::get_current_task();

    if (signum == SIGKILL) return -EINVAL;

    if (oldact) {
        self->write_to_userspace(oldact, &self->signal_actions[signum], sizeof(sigaction));        
    }

    if (act) {
        self->read_from_userspace(&self->signal_actions[signum], act, sizeof(sigaction));
    }

    return 0;
}

REGISTER_SYSCALL(SYS_sigaction, rt_sigaction);