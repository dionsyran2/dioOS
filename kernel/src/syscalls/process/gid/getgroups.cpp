#include <syscalls/syscalls.h>

long getgroups(int size, gid_t *list){
    task_t *self = task_scheduler::get_current_task();
    if (size == 0) return self->num_supplementary_groups;

    if (size < (sizeof(gid_t) * self->num_supplementary_groups)) return -EINVAL;

    self->write_to_userspace(list, &self->num_supplementary_groups, min(size, sizeof(self->num_supplementary_groups * sizeof(gid_t))));

    return self->num_supplementary_groups;
}

REGISTER_SYSCALL(SYS_getgroups, getgroups);