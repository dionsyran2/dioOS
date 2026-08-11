#include <syscalls/syscalls.h>
#include <memory.h>

uint64_t sys_mmap(uint64_t addr, size_t length, int prot, int flags, int fd, off_t offset){
    task_t *self = task_scheduler::get_current_task();
    if (!self || !self->vmm) return -EFAULT;

    // Basic validation
    if (length == 0) return -EINVAL;
    if (offset & 0xFFF) return -EINVAL; // Offset must be page-aligned

    length = ALIGN(length, 0x1000);
    addr = addr & ~0xFFFUL;

    uint64_t vm_flags = 0;
    if (prot & PROT_READ)  vm_flags |= VM_READ;
    if (prot & PROT_WRITE) vm_flags |= VM_WRITE;
    if (prot & PROT_EXEC)  vm_flags |= VM_EXEC;

    file_t *file = nullptr;
    if (flags & MAP_ANON) {
        vm_flags |= VM_ANON;
        fd = -1; // Anonymous maps ignore file descriptors
    } else {
        if (fd < 0) return -EBADF;
        file = self->fd_table->get_file(fd);
        if (!file) return -EBADF;
    }

    int errno = 0;
    void *m = nullptr;

    if (flags & MAP_FIXED) {
        // MAP_FIXED requires an explicit, non-zero, aligned address
        if (addr == 0) return -EINVAL;
        vm_flags |= VM_FIXED;
        
        m = self->vmm->mmap(addr, length, vm_flags, file ? file->node : nullptr, offset, errno);
    } else {
        if (addr){
            m = self->vmm->mmap(addr, length, vm_flags, file ? file->node : nullptr, offset, errno);
        } else {
            m = self->vmm->mmap(length, vm_flags, file ? file->node : nullptr, offset, errno);
        }
        
    }

    if (errno != 0 || m == nullptr) {
        return -errno;
    }

    return (uint64_t)m;
}

REGISTER_SYSCALL(SYS_mmap, sys_mmap);