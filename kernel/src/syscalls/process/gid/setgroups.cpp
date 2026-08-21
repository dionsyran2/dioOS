#include <syscalls/syscalls.h>

long sys_setgroups(size_t size, const gid_t *list) {
    task_t *self = task_scheduler::get_current_task();

    if (self->euid != 0) return -EPERM;

    if (size > NGROUPS_MAX) return -EINVAL;

    if (size > 0) {
        if (self->read_from_userspace(self->supplementary_groups, (void*)list, size * sizeof(gid_t)) < 0) {
            return -EFAULT;
        }
    }

    self->num_supplementary_groups = size;

    return 0;
}

REGISTER_SYSCALL(SYS_setgroups, sys_setgroups);