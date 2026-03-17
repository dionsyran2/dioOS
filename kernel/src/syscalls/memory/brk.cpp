#include <syscalls/syscalls.h>
#include <memory.h>
#include <paging/PageFrameAllocator.h>

uint64_t sys_brk(uint64_t brk){
    task_t* self = task_scheduler::get_current_task();

    if (brk > (BRK_DEFAULT_BASE + self->brk_offset)){
        uint64_t pages = DIV_ROUND_UP(brk - (BRK_DEFAULT_BASE + self->brk_offset), PAGE_SIZE);

        for (int i = 0; i < pages; i++){
            uint64_t kvirt = (uint64_t)GlobalAllocator.RequestPage();
            memset((void*)kvirt, 0, PAGE_SIZE);
            uint64_t physical = virtual_to_physical(kvirt);
            uint64_t virt = BRK_DEFAULT_BASE + self->brk_offset;

            self->vm_tracker.mark_allocation(virt, 0x1000, VM_FLAG_COW | VM_FLAG_US | VM_FLAG_RW);
            self->ptm->MapMemory((void*)virt, (void*)physical);
            self->ptm->SetFlag((void*)virt, PT_Flag::User, true);
            self->brk_offset += 0x1000;
        }
    } /*else {
        uint64_t pages = DIV_ROUND_UP((BRK_DEFAULT_BASE + self->brk_offset) - brk, PAGE_SIZE);

        for (int i = 0; i < pages; i++){
            if (self->brk_offset == 0) break;

            uint64_t virt = BRK_DEFAULT_BASE + self->brk_offset;

            self->vm_tracker.remove_allocation(virt, 0x1000);
            uint64_t physical = self->ptm->getPhysicalAddress((void*)virt);
            GlobalAllocator.DecreaseReferenceCount((void*)physical);

            self->ptm->Unmap((void*)virt);
            self->brk_offset -= 0x1000;
        }
    }*/

    return BRK_DEFAULT_BASE + self->brk_offset;
}
REGISTER_SYSCALL(SYS_brk, sys_brk);