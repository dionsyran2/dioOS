#include <syscalls/syscalls.h>
#include <bits/signals.h>

int sigreturn(){
    task_t *self = task_scheduler::get_current_task();

    asm ("cli");
    
    memcpy(&self->registers, self->syscall_registers, sizeof(__registers_t));
    self->restore_signal();
    return 0; // Unreachable
}


REGISTER_SYSCALL(SYS_sigreturn, sigreturn);