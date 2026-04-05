#include <syscalls/syscalls.h>
#include <memory.h>

int sys_sigaction(int signum, const kernel_sigaction *action, kernel_sigaction *oldact){
    signum--;
    if (signum > 63 || signum < 0) return -EINVAL;

    task_t *self = task_scheduler::get_current_task();

    if (oldact){
        self->write_memory(oldact, &self->signal_actions[signum], sizeof(kernel_sigaction));
    }

    if (action){
        kernel_sigaction act;
        self->read_memory(action, &act, sizeof(kernel_sigaction));

        // check if everything is supported... (TODO)

        memcpy(&self->signal_actions[signum], &act, sizeof(kernel_sigaction));
    }

    return 0;
}

REGISTER_SYSCALL(SYS_sigaction, sys_sigaction);