#include <syscalls/syscalls.h>
#include <syscalls/linux/memory.h>


unsigned long sys_brk(unsigned long brk){
    //serialf("sys_brk(%llx)\n\r", brk);
    task_t* task = task_scheduler::get_current_task();
    if (brk == 0) return task->brk_end;
    if (brk == task->brk_end) return task->brk_end;
    if (brk < task->brk_end) return task->brk_end; // maybe free the region instead of doing that?

    size_t size = brk - task->brk_end;
    size_t pages = size / 0x1000;
    if (size & 0xFFF) pages++;

    for (int i = 0; i < pages; i++){
        void* page = GlobalAllocator.RequestPage();
        if (page == nullptr) return 0; // run out of memory

        task->ptm->MapMemory((void*)task->brk_end, (void*)globalPTM.getPhysicalAddress(page));
        task->brk_end += 0x1000;
    }

    return brk;
}

#include <syscalls/linux/filesystem.h>

unsigned long sys_mmap(void *addr, size_t length, int prot, int flags, int fd, uint64_t offset){
    //serialf("sys_mmap(%llx, %u, %d, %d, %d, %u)\n\r", addr, length, prot, flags, fd, offset);
    task_t* ctask = task_scheduler::get_current_task();
    PageTableManager* ptm = ctask->ptm;
    
    if (addr == nullptr){
        addr = (void*)(*ctask->rmap_ptr);
        size_t pages = (length + 0xFFF) / 0x1000;
        (*ctask->rmap_ptr) += pages * 0x1000;
    }

    length += (size_t)addr & 0xFFF;

    addr = (void*)((uint64_t)addr & ~0xFFF);
    
    for (int i = 0; i < length; i += 0x1000){
        void* vaddr = (void*)((unsigned long)addr + i);
        bool mapped = ptm->isMapped(vaddr);
        if (mapped && (flags & MAP_FIXED) == 0)
            continue;

        void* page = GlobalAllocator.RequestPage();
        memset(page, 0, 0x1000);
        ptm->MapMemory(vaddr, (void*)globalPTM.getPhysicalAddress(page));
        ptm->SetPageFlag(vaddr, PT_Flag::UserSuper, true);
    }

    if (fd > 0){
        sys_pread64(fd, (char*)addr, length, offset);
    }
    return (unsigned long)addr;
}

int sys_unmap(void* addr, size_t length){
    //serialf("sys_unmap(%llx, %u)\n\r", addr, length);

    if (addr == nullptr) return -EINVAL;
    
    task_t* ctask = task_scheduler::get_current_task();
    PageTableManager* ptm = ctask->ptm;

    addr = (void*)((uint64_t)addr & ~0xFFF);
    for (int i = 0; i < length; i += 0x1000){
        void* vaddr = (void*)((unsigned long)addr + i);
        ptm->UnmapMemory(vaddr);
    }

    return 0;
}


int sys_mprotect(void* addr, size_t len, int prot){
    addr = (void*)((unsigned long)addr & ~(0x1000 - 1));
    len = (((unsigned long)addr + len + 0x1000 - 1) & ~(0x1000 - 1)) - (unsigned long)addr;
    PageTableManager* ptm = task_scheduler::get_current_task()->ptm;
    if (((unsigned long)addr & 0xFFF)) {
        //serialPrint(COM1, "UNALIGNED\n\r");
        //serialPrint(COM1, toHString((uint64_t)addr));

        return -1;
    };

    for (int i = 0; i < len; i += 0x1000){
        void* caddr = (void*)((unsigned long)addr + i);
        if (!ptm->isMapped(caddr)) {
            //serialPrint(COM1, "UNMAPPED");
            return -1;
        }
        if (prot & PROT_READ){
            ptm->SetPageFlag(caddr, PT_Flag::ReadWrite, false);
        }

        if (prot & PROT_WRITE){
            ptm->SetPageFlag(caddr, PT_Flag::ReadWrite, true);
        }

        if ((prot & PROT_EXEC) == 0){
            //ptm->SetPageFlag(caddr, PT_Flag::NX, true);
        }else{
            ptm->SetPageFlag(caddr, PT_Flag::NX, false);
        }
    }

    return 0;
}

int sys_mincore(uint64_t addr, size_t length, unsigned char *vec){ // determine whether pages are resident in memory
    if (addr & 0xFFF) return -EINVAL;

    task_t* ctask = task_scheduler::get_current_task();

    if (!ctask->ptm->isMapped(vec)) return -EFAULT;

    if (length & 0xFFF) length += 0x1000 - (length & 0xFFF);

    uint64_t end = (addr + length);
    for (uint64_t a = addr; a < end; a += 0x1000){
        if (ctask->ptm->isMapped((void*)a)){
            *vec |= 0x01;
        }else{
            *vec &= ~0x01;
        }
        vec++;
    }
    return 0;
}

void register_mem_syscalls(){
    register_syscall(SYSCALL_BRK, (syscall_handler_t)sys_brk);
    register_syscall(SYSCALL_MMAP, (syscall_handler_t)sys_mmap);
    register_syscall(SYSCALL_UNMAP, (syscall_handler_t)sys_unmap);
    register_syscall(SYSCALL_MPROTECT, (syscall_handler_t)sys_mprotect);
    register_syscall(SYSCALL_MINCORE, (syscall_handler_t)sys_mincore);

}