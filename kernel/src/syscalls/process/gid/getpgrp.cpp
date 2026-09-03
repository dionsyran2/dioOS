#include <syscalls/syscalls.h>

pid_t sys_getpgid(pid_t pid) {
    task_t *self = task_scheduler::get_current_task();
    if (pid == 0 || pid == self->pid) {
        return self->pgid;
    }

    task_t *target = task_scheduler::search_by_pid(pid);
    if (!target) return -ESRCH;

    if (target->sid != self->sid) return -EPERM;

    return target->pgid;
}

pid_t sys_getpgrp() {
    return sys_getpgid(0);
}

REGISTER_SYSCALL(SYS_getpgrp, sys_getpgrp);
REGISTER_SYSCALL(SYS_getpgid, sys_getpgid);