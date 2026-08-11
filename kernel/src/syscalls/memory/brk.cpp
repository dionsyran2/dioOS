#include <syscalls/syscalls.h>

uint64_t brk(uint64_t new_brk){
    task_t *self = task_scheduler::get_current_task();
    mm_struct_t *mm = self->vmm;

    // If its zero, return the current break
    if (new_brk == 0){
        return mm->current_brk;
    }

    uint64_t old_brk = mm->current_brk;

    // Security validation
    if (new_brk < mm->initial_brk || new_brk >= 0x00007FFFFFFFFFFF){
        return old_brk;
    }

    new_brk = ALIGN(new_brk, 0x1000);
    if (new_brk > old_brk) {
        // Grow the break
        if (new_brk > old_brk) {
            uint64_t size_to_allocate = new_brk - old_brk;
            int errno = 0;

            void *res = mm->mmap(old_brk, size_to_allocate, 
                                     VM_WRITE,
                                     nullptr, 0,
                                     errno);
            if (res == nullptr) {
                return old_brk; // Out of memory
            }
        }
    } else if (new_brk < old_brk) {
        // Shrink the break
        if (new_brk < old_brk) {
            uint64_t size_to_free = old_brk - new_brk;
            int errno = 0;

            // Release that part of the memory
            mm->free(new_brk, size_to_free, errno);
        }
    }

    // Update the task's official break tracker
    mm->current_brk = new_brk;
    return mm->current_brk;
}

REGISTER_SYSCALL(SYS_brk, brk);