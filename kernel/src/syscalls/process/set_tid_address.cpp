#include <syscalls/syscalls.h>

int64_t sys_set_tid_address(int *tidptr) {
    task_t *self = task_scheduler::get_current_task();
    
    // Save the pointer for when the thread exits
    self->clear_child_tid = tidptr;
    
    return self->pid; 
}

REGISTER_SYSCALL(SYS_set_tid_address, sys_set_tid_address);