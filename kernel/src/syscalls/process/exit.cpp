#include <syscalls/syscalls.h>

int exit(int status){
    task_t *self = task_scheduler::get_current_task();

    serialf("%d | exit(%d)\n\r", self->pid, status);
    self->exit(status);
    return 0; // Unreachable
}

REGISTER_SYSCALL(SYS_exit, exit);

/*
_exit() terminates the calling process "immediately". Any open file descriptors belonging to the process are closed. Any children of the process are inherited by init(1) (or by the nearest "subreaper" process as defined through the use of the prctl(2) PR_SET_CHILD_SUBREAPER operation). The process's parent is sent a SIGCHLD signal.

The value status & 0xFF is returned to the parent process as the process's exit status, and can be collected by the parent using one of the wait(2) family of calls.

The function _Exit() is equivalent to _exit().
*/

REGISTER_SYSCALL(SYS_exit_group, exit); // This system call terminates all threads in the calling process's thread group.