#include <syscalls/syscalls.h>
#include <memory.h>
#include <paging/PageFrameAllocator.h>
#include <syscalls/files/files.h>
int sys_mprotect(void* addr, size_t len, int prot);
int sys_unmap(void* addr, size_t length);

unsigned long sys_mmap(void *addr, size_t length, int prot, int flags, int fd, uint64_t offset){
    task_t* self = task_scheduler::get_current_task();
    size_t page_count = (length + 0xFFF) / 0x1000;
    length = page_count * 0x1000;

    PageTableManager* ptm = self->ptm;
    bool needs_load = (fd > 0);

    if (needs_load){
        fd_t *file = self->get_fd(fd);
        if (fd && file->node->file_operations.mmap) 
            return file->node->file_operations.mmap(addr, length, prot, flags, fd, offset, file->node);
    }

    bool find_new_addr = (addr == nullptr);
    
    if (!find_new_addr && (flags & MAP_FIXED) == 0) {
        for (size_t i = 0; i < length; i += 0x1000) {
            if (ptm->GetFlag((void*)((uint64_t)addr + i), PT_Flag::Present)) {
                find_new_addr = true;
                break;
            }
        }
    }

    if (find_new_addr) {
        addr = (void*)(RMAP_DEFAULT_BASE + self->vm_tracker->rmap_offset);
        self->vm_tracker->rmap_offset += length;
    } else if (flags & MAP_FIXED) {
        sys_unmap(addr, length);
    }

    for (size_t i = 0; i < length; i += 0x1000){
        void* vaddr = (void*)((unsigned long)addr + i);
        
        void* page = GlobalAllocator.RequestPage();
        if (page == nullptr) return -ENOMEM;

        uint64_t physical = virtual_to_physical((uint64_t)page);
        memset(page, 0, 0x1000);

        self->ptm->MapMemory(vaddr, (void*)physical);
        self->ptm->SetFlag(vaddr, PT_Flag::User, true);
    }

    uint32_t vflags = VM_FLAG_US | VM_FLAG_RW | ((flags & MAP_SHARED) ? VM_FLAG_SHARED : VM_FLAG_COW);
    self->vm_tracker->mark_allocation((uint64_t)addr, length, vflags);

    if (needs_load){
        int r = sys_pread64(fd, (char*)addr, length, offset);
        sys_mprotect(addr, length, prot);
        if (r < 0) return r;
    }
    
    return (uint64_t)addr;
}
REGISTER_SYSCALL(SYS_mmap, sys_mmap);


int sys_unmap(void* addr, size_t length){
    if (addr == nullptr) return -EINVAL;
    
    task_t* self = task_scheduler::get_current_task();
    PageTableManager* ptm = self->ptm;

    // Align length
    size_t page_count = (length + 0xFFF) / 0x1000;
    length = page_count * 0x1000;
    addr = (void*)((uint64_t)addr & ~0xFFF);

    for (size_t i = 0; i < length; i += 0x1000){
        void* vaddr = (void*)((unsigned long)addr + i);
        
        if (!ptm->GetFlag(vaddr, PT_Flag::Present)) continue;

        uint64_t phys = self->ptm->getPhysicalAddress(vaddr);

        self->vm_tracker->remove_allocation((uint64_t)vaddr, 0x1000);
        GlobalAllocator.DecreaseReferenceCount((void*)phys);
        ptm->Unmap(vaddr);
    }

    return 0;
}

REGISTER_SYSCALL(SYS_unmap, sys_unmap);


int sys_mprotect(void* addr, size_t len, int prot){
    /*const unsigned long PAGE_MASK = ~(PAGE_SIZE - 1);

    unsigned long aligned_addr = (unsigned long)addr & PAGE_MASK;

    unsigned long end_addr = (unsigned long)addr + len;
    
    unsigned long aligned_end_addr = (end_addr + PAGE_SIZE - 1) & PAGE_MASK;
    
    unsigned long aligned_len = aligned_end_addr - aligned_addr;
    
    task_t* self = task_scheduler::get_current_task();
    PageTableManager* ptm = self->ptm;

    for (unsigned long i = 0; i < aligned_len; i += PAGE_SIZE){
        void* caddr = (void*)(aligned_addr + i);
        
        if (ptm->GetFlag(caddr, PT_Flag::User) == false) return -EACCES;
        if (!ptm->GetFlag(caddr, PT_Flag::Present)) {
            return -1; 
        }

        int vflags = 0;
        
        if (prot & PROT_WRITE) {
            // Set W bit to allow writes (true)
            ptm->SetFlag(caddr, PT_Flag::Write, true);
            vflags |= VM_FLAG_RW;
        } else {
            // Clear W bit (Page becomes Read-Only)
            ptm->SetFlag(caddr, PT_Flag::Write, false);
        }

        if (prot & PROT_EXEC) {
            // Executable: Clear NX bit (false)
            ptm->SetFlag(caddr, PT_Flag::NX, false);
            vflags |= VM_FLAG_NX;
        } else {
            // Non-Executable: Set NX bit (true)
            ptm->SetFlag(caddr, PT_Flag::NX, true);
        }

        uint8_t old_flags = self->vm_tracker->get_flags((uint64_t)caddr);
        uint32_t new_flags = VM_FLAG_COW | VM_FLAG_US | vflags;
        
        if (old_flags & VM_PENDING_COW) {
            new_flags |= VM_PENDING_COW;
        }

        self->vm_tracker->set_flags((uint64_t)caddr, 0x1000, new_flags);
    }*/

    return 0;
}

REGISTER_SYSCALL(SYS_mprotect, sys_mprotect);
