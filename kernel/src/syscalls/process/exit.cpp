#include <syscalls/syscalls.h>

void exit(int status){
    task_t *self = task_scheduler::get_current_task();

    self->exit(status);
}

REGISTER_SYSCALL(60, exit);

void exit_group(int status){
    task_scheduler::exit_process(task_scheduler::get_current_task(), status);
}

REGISTER_SYSCALL(SYS_exitgroup, exit_group);