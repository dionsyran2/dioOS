#include <syscalls/syscalls.h>
#include <kerrno.h>

uint64_t sys_munmap(uint64_t addr, size_t length) {
    task_t *self = task_scheduler::get_current_task();
    if (!self || !self->vmm) return -EFAULT;

    if (addr % 0x1000 != 0 || length == 0) {
        return -EINVAL;
    }

    int errno = 0;
    self->vmm->free(addr, length, errno);

    return -errno;
}

REGISTER_SYSCALL(SYS_munmap, sys_munmap);