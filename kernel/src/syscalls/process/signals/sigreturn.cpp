#include <syscalls/syscalls.h>
#include <bits/signals.h>

int sigreturn(){
    task_t *self = task_scheduler::get_current_task();

    self->restore_signal();
    return 0; // Unreachable
}


REGISTER_SYSCALL(SYS_sigreturn, sigreturn);