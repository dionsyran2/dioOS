#include <syscalls/syscalls.h>
#include <memory.h>
#include <paging/PageFrameAllocator.h>

uint64_t sys_brk(uint64_t brk){
    task_t* self = task_scheduler::get_current_task();
    uint64_t current_brk = BRK_DEFAULT_BASE + self->vm_tracker->brk_offset;

    if (brk == 0 || brk == current_brk) {
        return current_brk;
    }

    if (brk > current_brk){
        uint64_t current_mapped_end = (current_brk + 0xFFF) & ~0xFFF;
        uint64_t new_mapped_end = (brk + 0xFFF) & ~0xFFF;

        if (new_mapped_end > current_mapped_end) {
            uint64_t size_to_map = new_mapped_end - current_mapped_end;

            self->vm_tracker->mark_allocation(current_mapped_end, size_to_map, VM_FLAG_COW | VM_FLAG_US | VM_FLAG_RW);

            for (uint64_t i = 0; i < size_to_map; i += 0x1000){
                uint64_t virt = current_mapped_end + i;
                
                uint64_t kvirt = (uint64_t)GlobalAllocator.RequestPage();
                memset((void*)kvirt, 0, PAGE_SIZE);
                uint64_t physical = virtual_to_physical(kvirt);

                self->ptm->MapMemory((void*)virt, (void*)physical);
                self->ptm->SetFlag((void*)virt, PT_Flag::User, true);
                self->ptm->SetFlag((void*)virt, PT_Flag::Write, true);
            }
        }
    }

    self->vm_tracker->brk_offset = brk - BRK_DEFAULT_BASE;

    return brk;
}

REGISTER_SYSCALL(SYS_brk, sys_brk);