#include <syscalls/syscalls.h>
#include <memory.h>
#include <signum.h>

int rt_sigreturn(){
    task_t *self = task_scheduler::get_current_task();
    
    asm ("cli");
    // Undo everything _handle_signal did.
    uint64_t registers_location = self->syscall_registers->rsp;
    if (!self->read_memory((void*)registers_location, &self->registers, sizeof(__registers_t))){
        self->exit(SIGSEGV);
    }

    if (!self->read_memory((void *)(registers_location + sizeof(__registers_t)), self->saved_fpu_state, g_fpu_storage_size)){
        self->exit(SIGSEGV);
    }

    self->executing_syscall = false;
    self->signal_count--;

    if (self->signal_count == 0){
        self->signal_mask = self->saved_signal_mask;
    }

    restore_fpu_state(self->saved_fpu_state);
    _execute_task(&self->registers);
    return 0; // Unreachable (_execute_task will change execution context)
}

REGISTER_SYSCALL(SYS_rt_sigreturn, rt_sigreturn);