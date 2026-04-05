#include <syscalls/syscalls.h>

long set_tid_address(unsigned long addr){
    task_t* self = task_scheduler::get_current_task(); 
    
    self->clear_child_tid = (uint32_t*)addr;

    return (long)self->pid; 
}

REGISTER_SYSCALL(SYS_set_tid_address, set_tid_address);