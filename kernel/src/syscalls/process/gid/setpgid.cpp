#include <syscalls/syscalls.h>

int sys_setpgid(pid_t pid, pid_t pgid) {
    task_t *self = task_scheduler::get_current_task();
    task_t *target = self;

    if (pid != 0 && pid != self->pid) {
        target = task_scheduler::search_by_pid(pid);
        if (!target) return -ESRCH;
    }

    // Session leaders cannot change process group
    if (target->sid == target->pid) return -EPERM;

    // Target must belong to the caller's session
    if (target->sid != self->sid) return -EPERM;

    if (pgid == 0) {
        pgid = target->pid;
    } else if (pgid < 0) {
        return -EINVAL;
    }

    // If joining an existing group, ensure that group exists in the session
    if (pgid != target->pid) {
        task_t *leader = task_scheduler::search_by_pid(pgid);
        if (!leader || leader->sid != self->sid) {
            return -EPERM;
        }
    }

    target->pgid = pgid;
    return 0;
}

REGISTER_SYSCALL(SYS_setpgid, sys_setpgid);