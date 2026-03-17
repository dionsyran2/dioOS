#include <syscalls/syscalls.h>

void exit_group(int status){
    task_scheduler::exit_process(task_scheduler::get_current_task(), status);
}

REGISTER_SYSCALL(SYS_exitgroup, exit_group);