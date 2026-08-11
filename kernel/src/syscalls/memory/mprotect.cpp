#include <syscalls/syscalls.h>
#include <memory.h>

uint64_t sys_mprotect(uint64_t addr, size_t length, int prot) {
    task_t *self = task_scheduler::get_current_task();
    if (!self || !self->vmm) return -EFAULT;

    if (addr % 0x1000 != 0) return -EINVAL;
    if (length == 0) return 0;

    uint64_t vm_flags = 0;
    if (prot & PROT_READ)  vm_flags |= VM_READ;
    if (prot & PROT_WRITE) vm_flags |= VM_WRITE;
    if (prot & PROT_EXEC)  vm_flags |= VM_EXEC;

    int err = self->vmm->mprotect(addr, length, vm_flags);
    
    return err;
}

REGISTER_SYSCALL(SYS_mprotect, sys_mprotect);