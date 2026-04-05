#include <scheduling/task_scheduler/task_scheduler.h>
#include <paging/PageFrameAllocator.h>
#include <interrupts/interrupts.h>
#include <memory.h>
#include <memory/heap.h>
#include <math.h>
#include <cstr.h>
#include <local.h>
#include <syscalls/files/fcntl.h>
#include <drivers/timers/common.h>
#include <signum.h>

#define UTILIZATION_UPDATE_FREQUENCY 1000 // Every 1000 ticks (ms)

/* TASK HELPERS */
fd_t* task_t::get_fd(int num, bool lock){
    uint64_t rflags = 0;
    if (lock) rflags = spin_lock(&file_descriptor_lock);

    for (fd_t* fd = this->file_descriptors; fd != nullptr; fd = fd->next){
        if (fd->num == num) {
            if (lock) spin_unlock(&file_descriptor_lock, rflags);
            return fd;
        }
    }

    if (lock) spin_unlock(&file_descriptor_lock, rflags);
    return nullptr;
}

fd_t* task_t::open_node(vnode_t* node, int num){
    /* Create the file entry in /proc */
    char buff[128];
    stringf(buff, sizeof(buff), "/proc/%d/fd/%d", this->pid, num);

    vnode_t *file = vfs::create_path(buff, VLNK);

    if (file){
        char* str = vfs::get_full_path_name(node);
        file->write(0, strlen(str), str);

        free(str);
        file->close();
    }

    // Open the actual file descriptor
    uint64_t rflags = spin_lock(&file_descriptor_lock);

    while(1){
        fd_t* fd = get_fd(num, false);
        if (!fd) break;

        num++;
    }

    fd_t *fd = new fd_t;
    fd->next = nullptr;
    fd->node = node;
    fd->num = num;
    fd->offset = 0;
    fd->flags = O_RDWR;
    fd->owner = this;

    if (this->file_descriptors == nullptr){
        this->file_descriptors = fd;
    } else {
        fd_t *c = this->file_descriptors;
        while (c->next) c = c->next;

        c->next = fd;
    }

    spin_unlock(&file_descriptor_lock, rflags);
    
    return fd;
}

void task_t::close_fd(int num){
    fd_t *fd_check = this->get_fd(num);
    if (!fd_check) return;

    char buff[128];
    stringf(buff, sizeof(buff), "/proc/%d/fd/%d", this->pid, num);

    vnode_t *entry = vfs::resolve_path(buff, false, false);
    if (entry){
        entry->unlink();
        entry->close();
    }

    uint64_t rflags = spin_lock(&file_descriptor_lock);


    fd_t* prev = nullptr;
    for (fd_t* fd = this->file_descriptors; fd != nullptr; fd = fd->next){
        if (fd->num == num) {
            spin_unlock(&file_descriptor_lock, rflags);
            if (prev) {
                prev->next = fd->next;
            } else {
                this->file_descriptors = fd->next;
            }
            if (fd->node) fd->node->close();
            delete fd;
            break;
        }
        
        prev = fd;
    }

    spin_unlock(&file_descriptor_lock, rflags);
}
/*
void* task_t::AllocatePage(bool COW){
    void* page = GlobalAllocator.RequestPage();
    vm_tracker.mark_allocation((uint64_t)page, PAGE_SIZE, VM_FLAG_RW | (COW ? VM_FLAG_COW : 0));
    return page;
}

void* task_t::AllocatePages(size_t pages, bool COW){
    void* page = GlobalAllocator.RequestPages(pages);
    if (COW) vm_tracker.mark_allocation((uint64_t)page, pages * PAGE_SIZE, VM_FLAG_RW | VM_FLAG_US | (COW ? VM_FLAG_COW : 0));
    return page;
}

void task_t::UnallocatePage(void* page){
    uint64_t physical = ptm != nullptr ? ptm->getPhysicalAddress(page) : virtual_to_physical((uint64_t)page);
    vm_tracker.remove_allocation((uint64_t)page, PAGE_SIZE);

    GlobalAllocator.DecreaseReferenceCount((void*)physical);
}
*/
void task_t::Block(task_block_type_t type, uint64_t context){
    this->schedule_until = 0;
    this->block_type = type;
    this->block_context = context;

    this->previous_state = this->task_state;
    this->task_state = BLOCKED;

    task_scheduler::_swap_tasks();
}

void task_t::Unblock(){
    this->task_state = PAUSED;
}


/* @brief Sets the task state for specified ms, then reverts back. Used mainly for blocking */
/* @param ms: The time (in milliseconds, -1 means indefinetly. Use it only for blocking when waiting on IO) */
/* @param block_type: The block type (Optional) */
/* @param block_context: The context of the block*/
void task_t::ScheduleFor(int64_t ms, task_state_t state, task_block_type_t block_type, uint64_t block_context){
    uint64_t final_ticks = local_apic_list->tick_count + (ms * local_apic_list->ticks_per_ms); // According to the first lapic on the list (bsp)

    this->schedule_until = final_ticks;
    this->block_type = block_type;
    this->block_context = block_context;

    this->previous_state = this->task_state;
    this->task_state = state;

    task_scheduler::_swap_tasks();
}


// @brief Read from process memory
bool task_t::read_memory(const void* address, void* buffer, size_t length){
    size_t bytes_copied = 0;
    PageTableManager* PTM = this->ptm ? this->ptm : &globalPTM;

    while (bytes_copied < length) {
        uint64_t user_va = (uint64_t)address + bytes_copied;
        uint64_t page_offset = user_va & 0xFFF; // Offset within the 4KB page
        
        size_t chunk_size = 0x1000 - page_offset; // Bytes left in this page
        size_t remaining = length - bytes_copied;
        if (chunk_size > remaining) chunk_size = remaining;

        uint64_t phys = PTM->getPhysicalAddress((void*)user_va);
        
        if (phys == 0) return false;
        if (!PTM->GetFlag((void*)(user_va & (~0xFFFUL)), PT_Flag::User)) return false; // Not user accessible


        void* kernel_src = (void*)(physical_to_virtual(phys) + page_offset);

        memcpy((void*)((uint64_t)buffer + bytes_copied), kernel_src, chunk_size);

        bytes_copied += chunk_size;
    }

    return true;
}

// @brief Write to process memory safely, respecting Copy-On-Write
bool task_t::write_memory(void* address, const void* buffer, size_t length){
    size_t bytes_copied = 0;
    PageTableManager* PTM = this->ptm ? this->ptm : &globalPTM;

    while (bytes_copied < length) {
        uint64_t user_va = (uint64_t)address + bytes_copied;
        uint64_t page_offset = user_va & 0xFFF;
        uint64_t pg = user_va & ~0xFFFUL; // Base page address
        
        size_t chunk_size = 0x1000 - page_offset;
        size_t remaining = length - bytes_copied;
        if (chunk_size > remaining) chunk_size = remaining;


        uint64_t flags = this->vm_tracker->get_flags(pg);
        if (flags && flags & VM_PENDING_COW) {
            uint64_t old_physical = PTM->getPhysicalAddress((void*)pg);
            if (old_physical) {
                // Allocate a new page for the split
                void* new_page = GlobalAllocator.RequestPage();
                if (!new_page) {
                    kprintf("\nWRITE_MEMORY FAULT (new_page == NULL)\n");
                    return false;
                }

                // Copy the data via the kernel's high-memory mapping
                memcpy(new_page, (void*)physical_to_virtual(old_physical), PAGE_SIZE);

                // Remove old reference from the shared page
                GlobalAllocator.DecreaseReferenceCount((void*)old_physical);

                // Map the new private page into this task's page tables
                uint64_t new_physical = virtual_to_physical((uint64_t)new_page);
                PTM->MapMemory((void*)pg, (void*)new_physical);

                // Restore all standard permissions
                if (flags & VM_FLAG_RW) PTM->SetFlag((void*)pg, PT_Flag::Write, true);
                if (flags & VM_FLAG_NX) PTM->SetFlag((void*)pg, PT_Flag::NX, true);
                if (flags & VM_FLAG_CD) PTM->SetFlag((void*)pg, PT_Flag::CacheDisable, true);
                if (flags & VM_FLAG_WT) PTM->SetFlag((void*)pg, PT_Flag::WriteThrough, true);
                PTM->SetFlag((void*)pg, PT_Flag::User, true);

                // Clear the CoW flag so we don't split it again
                this->vm_tracker->set_flags(pg, PAGE_SIZE, flags & ~VM_PENDING_COW);
            }
        }

        uint64_t phys = PTM->getPhysicalAddress((void*)user_va);
        if (phys == 0) {
            kprintf("\nWRITE_MEMORY FAULT (phys == 0) (%p)\n", user_va);
            return false;
        }
        
        if (!PTM->GetFlag((void*)(user_va & (~0xFFFUL)), PT_Flag::User)) {
            kprintf("\nACCESS FAULT\n");
            return false;
        } // Not user accessible

        void* kernel_dst = (void*)(physical_to_virtual(phys) + page_offset);

        memcpy(kernel_dst, (void*)((uint64_t)buffer + bytes_copied), chunk_size);

        bytes_copied += chunk_size;
    }

    return true;
}

// @brief Reads a null-terminated string
char* task_t::read_string(void* address, size_t max_length) {
    char* kernel_buffer = (char*)malloc(max_length + 1);
    if (!kernel_buffer) return nullptr;

    size_t bytes_read = 0;
    PageTableManager* PTM = this->ptm ? this->ptm : &globalPTM;

    while (bytes_read < max_length) {
        uint64_t user_va = (uint64_t)address + bytes_read;
        uint64_t page_offset = user_va & 0xFFF;
        
        size_t chunk_size = 0x1000 - page_offset;
        if (chunk_size > (max_length - bytes_read)) chunk_size = max_length - bytes_read;

        uint64_t phys = PTM->getPhysicalAddress((void*)user_va);
        
        if (phys == 0) break;
        if (!PTM->GetFlag((void*)(user_va & (~0xFFFUL)), PT_Flag::User)) break; // Not user accessible

        char* kernel_src = (char*)(physical_to_virtual(phys) + page_offset);

        // Scan this chunk for a null terminator
        bool null_found = false;
        for (size_t i = 0; i < chunk_size; i++) {
            char c = kernel_src[i];
            kernel_buffer[bytes_read + i] = c;
            
            if (c == '\0') {
                bytes_read += i;
                goto finished;
            }
        }

        bytes_read += chunk_size;
    }

finished:
    kernel_buffer[bytes_read] = '\0';
    return kernel_buffer;
}

#include <syscalls/linux/futex.h>
#include <drivers/serial/serial.h>

long futex_wake(uint32_t *uaddr, int futex_op, uint32_t val);

void task_t::exit(int status, bool silent){
    asm ("cli");

    if (!silent){
        if (this->clear_child_tid){
            uint32_t buffer = 0;

            this->write_memory(this->clear_child_tid, &buffer, sizeof(uint32_t));

            futex_wake((uint32_t*)this->ptm->getPhysicalAddress(this->clear_child_tid), FUTEX_WAKE, UINT32_MAX);
        }
        
        /*if (this->signal_parent_on_exit){
            task_t *parent = task_scheduler::get_process(this->ppid);

            if (parent){
                parent->kernel_signals |= (1 << (SIGCHLD - 1));
                parent->pending_signals |= (1 << (SIGCHLD - 1));
            }
        }*/
    }
    

    cpu_local_data *local = get_cpu_local_data();
    asm ("mov %0, %%rsp" :: "r" (local->scheduler_stack));

    for (fd_t *fd = this->file_descriptors; fd != nullptr;){
        fd_t* ofd = fd;
        fd = fd->next;
        if (ofd->owner == this) close_fd(ofd->num);
    }

    this->status_code = (status << 8);

    this->task_state = ZOMBIE;

    // -----
    GlobalAllocator.FreePages((void*)(this->syscall_stack_top - TASK_STACK_SIZE), DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE));
    GlobalAllocator.FreePages((void*)(this->kernel_stack_top - TASK_STACK_SIZE), DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE));
    
    if (this->userspace){
        this->vm_tracker->set_flags(this->user_stack_top - TASK_STACK_SIZE, TASK_STACK_SIZE, VM_FLAG_RW | VM_FLAG_US);
    }
    
    // Clear any memory allocated by the task (does not include the kernel stacks)
    this->vm_tracker->free(this->ptm);

    // Clear the fpu save
    uint64_t required_pages = DIV_ROUND_UP(g_fpu_storage_size, PAGE_SIZE);
    GlobalAllocator.FreePages(this->saved_fpu_state, required_pages);
    
    
    if (!silent) {
        task_scheduler::_wake_waiting_tasks(this->pid);
        serialf("EXIT pid: %d, tgid: %d, status: %d.\n", pid, tgid, status);
    }

    bool should_swap = task_scheduler::get_current_task() == this;
    
    get_cpu_local_data()->current_task = nullptr;
    if (this->is_krnl || silent) {
        task_scheduler::_remove_task_from_list(this);
        delete this;
    }

    if (should_swap && !silent){
        asm ("sti");
        task_scheduler::_swap_tasks();
        while(1);
    }
}

namespace task_scheduler
{
    spinlock_t task_list_lock;
    task_t* task_list;

    uint64_t current_pid = 10000;

    void mark_ready(task_t* task){
        task->is_ready = true;
    }

    void idle_function(){
        __asm__ ("sti");
        while(1) __asm__ ("hlt");
    }

    task_t* get_process(pid_t pid){
        uint64_t rflags = spin_lock(&task_list_lock);
        for (task_t* current = task_list; current != nullptr; current = current->next){
            if (current->pid == pid) {
                spin_unlock(&task_list_lock, rflags);
                return current;
            }
        }

        spin_unlock(&task_list_lock, rflags);
        return nullptr;
    }

    task_t *get_thread(tid_t tgid, pid_t pid){
        uint64_t rflags = spin_lock(&task_list_lock);
        for (task_t* current = task_list; current != nullptr; current = current->next){
            if (current->pid == pid && current->tgid == tgid) {
                spin_unlock(&task_list_lock, rflags);
                return current;
            }
        }
        
        spin_unlock(&task_list_lock, rflags);
        return nullptr;
    }

    // @brief It will initialize the scheduler (core specific)
    void initialize_scheduler(){
        cpu_local_data* local = get_cpu_local_data();
        local->idle_task = create_process("idle task", (function)idle_function, false, false);
        local->scheduler_stack = (uint64_t)GlobalAllocator.RequestPage() + PAGE_SIZE;
        
        _remove_task_from_list(local->idle_task);

        if (local->cpu_id == 0){
            _init_cpu_proc_fs();
        }
    }

    // @brief Returns the current task running on this cpu
    task_t* get_current_task(){
        cpu_local_data* local = get_cpu_local_data();
        return local->current_task;
    }

    // @brief Triggers an interrupt to swap tasks
    void _swap_tasks(){
        asm ("sti");
        asm ("int %0" : : "i" (SCHEDULER_SWAP_TASKS_VECTOR));
    }
    
    // @brief It will wake any tasks waiting on this pid
    void _wake_waiting_tasks(pid_t pid){
        uint64_t rflags = spin_lock(&task_list_lock);

        for (task_t* task = task_list; task != nullptr; task = task->next){
            if (task->task_state == BLOCKED && task->block_type == WAIT_FOR_CHILD
                && (task->block_context == -1 || task->block_context == pid))
            {
                task->task_state = PAUSED; // Wake the task up
            }

        }
        spin_unlock(&task_list_lock, rflags);
    }
    
    void _add_task_to_list(task_t* task){
        uint64_t rflags = spin_lock(&task_list_lock);

        task_t* c = task_list;
        task->next = nullptr;

        if (task_list == nullptr){
            task_list = task;
            goto add_task_to_list_exit;
        }

        while(c->next) c = c->next;
        c->next = task;
        
        add_task_to_list_exit:
        spin_unlock(&task_list_lock, rflags);
    }

    void _remove_task_from_list(task_t* task){
        if (task->proc_vfs_dir){
            task->proc_vfs_dir->rmdir();
            task->proc_vfs_dir = nullptr;
        }

        uint64_t rflags = spin_lock(&task_list_lock);
        
        task_t* prev = nullptr;
        task_t* current = task_list;

        while (current){
            if (current == task){
                if (prev == nullptr) {
                    task_list = current->next;
                } else {
                    prev->next = current->next;
                }
                break;
            }

            prev = current;
            current = current->next;
        }

        spin_unlock(&task_list_lock, rflags);
    }

    void exit_process(task_t* thread, int status){
        // Lock the list to prevent concurrent modifications
        uint64_t rflags = spin_lock(&task_list_lock);

        for (task_t* c = task_list; c != nullptr; c = c->next){
            if (c->pgid == thread->pgid && c != thread && c->task_state != ZOMBIE){
                
                c->kernel_signals |= (1 << (SIGKILL - 1));
                c->pending_signals |= (1 << (SIGKILL - 1));

                if (c->task_state == BLOCKED) {
                    c->task_state = PAUSED;
                }
            }
        }

        spin_unlock(&task_list_lock, rflags);

        // Now it is perfectly safe for THIS thread to kill itself
        thread->exit(status);
    }

    // @warning Returns the top of the stack (Allocates a stack)
    uint64_t _allocate_stack(task_t* task, uint64_t stack_top){
        if (!task->ptm) return (uint64_t)GlobalAllocator.RequestPages(DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE)) + TASK_STACK_SIZE;

        uint64_t base = stack_top - TASK_STACK_SIZE;

        
        uint64_t ret = 0;
        for (uint64_t o = 0; o < TASK_STACK_SIZE; o += PAGE_SIZE){
            uint64_t allocation = (uint64_t)GlobalAllocator.RequestPage();
            memset((void*)allocation, 0, PAGE_SIZE);

            uint64_t physical = virtual_to_physical(allocation);

            task->ptm->MapMemory((void*)(base + o), (void*)physical);
            task->ptm->SetFlag((void*)(base + o), PT_Flag::User, true);
            task->vm_tracker->mark_allocation(base + o, PAGE_SIZE, VM_FLAG_COW | VM_FLAG_RW | VM_FLAG_US);

            ret = base + o;
        }

        return ret;
    }

    void _init_task_fpu(task_t* task) {
        uint64_t required_pages = DIV_ROUND_UP(g_fpu_storage_size, PAGE_SIZE);
        task->saved_fpu_state = GlobalAllocator.RequestPages(required_pages);

        memset(task->saved_fpu_state, 0, g_fpu_storage_size);

        //  Set the Legacy Control Word (FCW) at offset 0
        //  Default value: 0x037F (Round to nearest, Mask all exceptions)
        uint16_t* fcw = (uint16_t*)task->saved_fpu_state;
        *fcw = 0x037F;

        // Set the MXCSR (SSE Control/Status) at offset 24 (0x18)
        uint32_t* mxcsr = (uint32_t*)((uint8_t*)task->saved_fpu_state + 24);
        *mxcsr = 0x1F80;

    }

    task_t* _create_task_structure(const char* name, function entry, bool userspace, bool create_ptm, bool init){
        bool intr = are_interrupts_enabled();
        asm ("cli");

        task_t* task = new task_t;
        memset(task, 0, sizeof(task_t));

        task->vm_tracker = new vm_tracker_t;
        task->start_time = GetTicks();

        PageTableManager* ptm = &globalPTM;

        if (create_ptm){
            ptm = new PageTableManager((PageTable*)GlobalAllocator.RequestPage(), task->vm_tracker);

            task->vm_tracker->mark_allocation((uint64_t)ptm->PML4, PAGE_SIZE, VM_FLAG_RW | VM_FLAG_DO_NOT_SHARE);
            serialf("task: %s\n", name);

            memset(ptm->PML4, 0, PAGE_SIZE);

            ClonePTM(ptm, &globalPTM);
        }

        memcpy(task->name, name, min(strlen(name), TASK_NAME_SIZE));
        memcpy(task->executable_name, name, min(strlen(name), TASK_NAME_SIZE));


        task->ptm = ptm;
        task->entry = entry;
        task->userspace = userspace;
        task->is_krnl = !userspace;
        task->is_ready = false;

        task->counter = TASK_SCHED_DEFAULT_COUNT;

        task->kernel_stack_top = (uint64_t)GlobalAllocator.RequestPages(DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE)) + TASK_STACK_SIZE;
        if (userspace && create_ptm) {
            task->user_stack_top = _allocate_stack(task, 0x800000000000);
        } else {
            task->user_stack_top = task->kernel_stack_top;
        }
        task->syscall_stack_top = (uint64_t)GlobalAllocator.RequestPages(DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE)) + TASK_STACK_SIZE;
        for (int i = 0; i < TASK_STACK_SIZE; i += 0x1000){
            task->ptm->SetFlag((void*)(task->syscall_stack_top - i), PT_Flag::User, true);
        }

        task->affinity = UINT64_MAX; // Enable all cores

        if (!init){
            task->tgid = task->pid = __atomic_fetch_add(&current_pid, 1, __ATOMIC_SEQ_CST);
            task->pgid = task->pid;
        } else {
            task->pid = task->tgid = task->pgid = 1;
        }

        vnode_t *root = vfs::resolve_path("/");
        task->cwd = task->open_node(root, 0x99);

        _init_task_fpu(task);
        _add_task_to_list(task);

        if (intr) asm ("sti");

        return task;
    }

    task_t* create_process(const char* name, function entry, bool userspace, bool ptm, bool init){
        task_t* task = _create_task_structure(name, entry, userspace, ptm, init);

        if (userspace){

            char full_path[256];
            stringf(full_path, sizeof(full_path), "/proc/%d", task->pid);

            task->proc_vfs_dir = vfs::create_path(full_path, VDIR);
            if (task->proc_vfs_dir){
                task->proc_vfs_dir->uid = task->ruid;
                task->proc_vfs_dir->gid = task->rgid;
                task->proc_vfs_dir->close();
            }

            _add_dynamic_task_virtual_files(task);
        }

        return task;
    }

    task_t* fork_process(task_t* process, bool share_vm, bool share_files){
        task_t* task = _create_task_structure(process->name, nullptr, true, false, false);

        memcpy(task->saved_fpu_state, process->saved_fpu_state, g_fpu_storage_size);

        if (!share_vm){
            task->ptm = new PageTableManager((PageTable*)GlobalAllocator.RequestPage(), task->vm_tracker);
            memset(task->ptm->PML4, 0, PAGE_SIZE);
            ClonePTM(task->ptm, process->ptm);

            task->vm_tracker->brk_offset = process->vm_tracker->brk_offset;
            task->vm_tracker->rmap_offset = process->vm_tracker->rmap_offset;
        } else {
            task->ptm = process->ptm;
            delete task->vm_tracker;

            task->vm_tracker = process->vm_tracker;
            task->vm_tracker->share_vm();
        }

        /* Copy the FDs */
        /*if (share_files){
            task->file_descriptors = process->file_descriptors;
        } else {*/
            for (fd_t* fd = process->file_descriptors; fd != nullptr; fd = fd->next){
                fd->node->open();
                task->open_node(fd->node, fd->num);
            }
        //}

        if (process->cwd) task->cwd = task->get_fd(process->cwd->num);
    
        task->ruid = process->ruid;
        task->rgid = process->rgid;
        task->euid = process->euid;
        task->egid = process->egid;
        task->suid = process->suid;
        task->sgid = process->sgid;


        /* Copy the registers */
        task->ppid = process->pid;
        task->registers.r15 = process->syscall_registers->r15;
        task->registers.r14 = process->syscall_registers->r14;
        task->registers.r13 = process->syscall_registers->r13;
        task->registers.r12 = process->syscall_registers->r12;
        task->registers.r11 = process->syscall_registers->r11;
        task->registers.r10 = process->syscall_registers->r10;
        task->registers.r9 = process->syscall_registers->r9;
        task->registers.r8 = process->syscall_registers->r8;

        task->registers.rbp = process->syscall_registers->rbp;
        task->registers.rdi = process->syscall_registers->rdi;
        task->registers.rsi = process->syscall_registers->rsi;
        task->registers.rdx = process->syscall_registers->rdx;
        task->registers.rcx = process->syscall_registers->rcx;
        task->registers.rbx = process->syscall_registers->rbx;
        task->registers.rsp = process->syscall_registers->rsp;
        task->registers.rip = process->syscall_registers->rcx;
        task->registers.rax = 0;
        
        task->registers.rflags = process->syscall_registers->r11;


        task->fs_pointer = process->fs_pointer;

        task->entry = (function)task->registers.rip;

        task->registers.cr3 = virtual_to_physical((uint64_t)task->ptm->PML4);

        return task;
    }

    task_t* clone_for_execve(task_t* victim){
        bool intr = are_interrupts_enabled();
        asm ("cli");

        task_t* task = new task_t;
        memset(task, 0, sizeof(task_t));

        task->vm_tracker = new vm_tracker_t();
        task->start_time = GetTicks();
        
        PageTableManager* ptm = new PageTableManager((PageTable*)GlobalAllocator.RequestPage(), task->vm_tracker);
        task->vm_tracker->mark_allocation((uint64_t)ptm->PML4, PAGE_SIZE, VM_FLAG_RW | VM_FLAG_DO_NOT_SHARE);
        
        memset(ptm->PML4, 0, PAGE_SIZE);
        ClonePTM(ptm, &globalPTM);

        memcpy(task->name, victim->name, strlen(victim->name));


        task->ptm = ptm;
        task->userspace = victim->userspace;
        task->is_krnl = victim->is_krnl;
        task->is_ready = false;

        task->signal_parent_on_exit = victim->signal_parent_on_exit;

        task->counter = TASK_SCHED_DEFAULT_COUNT;

        task->kernel_stack_top = (uint64_t)GlobalAllocator.RequestPages(DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE)) + TASK_STACK_SIZE;
        if (task->userspace) {
            task->user_stack_top = _allocate_stack(task, 0x800000000000);
        } else {
            task->user_stack_top = task->kernel_stack_top;
        }
        task->syscall_stack_top = (uint64_t)GlobalAllocator.RequestPages(DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE)) + TASK_STACK_SIZE;
        for (int i = 0; i < TASK_STACK_SIZE; i += 0x1000){
            task->ptm->SetFlag((void*)(task->syscall_stack_top - i), PT_Flag::User, true);
        }

        task->affinity = UINT64_MAX; // Enable all cores

        task->pid = victim->pid;
        task->ppid = victim->ppid;
        task->tgid = victim->tgid;
        task->pgid = victim->pgid;
        task->sid = victim->sid;

        task->ruid = victim->ruid;
        task->rgid = victim->rgid;
        task->euid = victim->euid;
        task->egid = victim->egid;
        task->suid = victim->suid;
        task->sgid = victim->sgid;

        if (victim->proc_vfs_dir){
            victim->proc_vfs_dir->rmdir();
            victim->proc_vfs_dir = nullptr;
        }

        char full_path[256];
        stringf(full_path, sizeof(full_path), "/proc/%d", task->pid);

        task->proc_vfs_dir = vfs::create_path(full_path, VDIR);
        if (task->proc_vfs_dir){
            task->proc_vfs_dir->uid = task->ruid;
            task->proc_vfs_dir->gid = task->rgid;
            task->proc_vfs_dir->close();
        }
        _add_dynamic_task_virtual_files(task);

        for (fd_t* fd = victim->file_descriptors; fd != nullptr; fd = fd->next){
            if (fd->flags & O_CLOEXEC) continue;
            fd->node->open();
            task->open_node(fd->node, fd->num);
        }

        bool set_cwd_to_root = false;
        if (victim->cwd){
            task->cwd = task->get_fd(victim->cwd->num);
            if (!task->cwd)
                set_cwd_to_root = true;
        } else {
            set_cwd_to_root = true;
        }

        if (set_cwd_to_root){
            vnode_t *root = vfs::resolve_path("/");

            task->cwd = task->open_node(root, 0x99);
        }
        
        _init_task_fpu(task);
        _add_task_to_list(task);

        if (intr) asm ("sti");

        return task;
    }

    void _handle_signals(task_t *task){
        if (!task->pending_signals) return;
    
        kernel_sigset_t signal_set = task->pending_signals;

        for (int i = 0; i < 64; i++){
            if ((signal_set & (1UL << i)) == 0 || (task->signal_mask & (1UL << i))) continue;

            // Signal is set.

            // Default
            serialf("Signal : %d %d\n", i + 1, task->signal_actions[i].sa_handler);

            int signal = i + 1;
            if (task->signal_actions[i].sa_handler == SIG_DFL){
                if (signal == SIGHUP || signal == SIGINT || signal == SIGKILL || signal == SIGTERM || 
                    signal == SIGABRT || signal == SIGSEGV || signal == SIGILL || signal == SIGFPE || 
                    signal == SIGBUS || signal == SIGQUIT) {
                    serialf("Task %d terminated by fatal signal %d\n", task->pid, signal);

                    task->exit(signal);
                    _swap_tasks();
                }
            } else if (task->signal_actions[i].sa_handler == SIG_IGN){
                // Ignore
            } else if (task->signal_actions[i].sa_handler == SIG_ERR /* What does this mean? */){
                // ???
            } else {
                // Run the handler
                bool siginfo = (task->signal_actions[i].sa_flags & SA_SIGINFO) != 0;

                __registers_t saved_regs;

                // Store the previous register values
                memcpy(&saved_regs, &task->registers, sizeof(__registers_t));

                // Jump over the red zone
                task->registers.rsp -= 128;

                if (siginfo){
                    siginfo_t info;
                    memset(&info, 0, sizeof(info));

                    info.si_signo = i + 1; 
                    info.si_code = (task->kernel_signals & (1UL << i)) ? SI_KERNEL : SI_USER;
                    
                    task->registers.rsp -= sizeof(info);
                    task->write_memory((void*)task->registers.rsp, &info, sizeof(info));
                    
                    task->registers.rsi = task->registers.rsp; // Arg 2: siginfo pointer
                    task->registers.rdx = 0x1234DEAD;          // Arg 3: ucontext pointer (TODO)
                }

                // Create space for the registers
                task->registers.rsp -= (sizeof(__registers_t) + g_fpu_storage_size);
                
                // Align the stack to 16 bytes
                task->registers.rsp &= ~0xF; 

                // Save the registers
                task->write_memory((void *)task->registers.rsp, &saved_regs, sizeof(__registers_t));
                task->write_memory((void *)(task->registers.rsp + sizeof(__registers_t)), task->saved_fpu_state, g_fpu_storage_size);

                // Push the restorer
                task->registers.rsp -= sizeof(uint64_t);
                task->write_memory((void*)task->registers.rsp, &task->signal_actions[i].sa_restorer, sizeof(uint64_t));

                task->registers.rdi = i + 1; // Arg 1: Signal number
                task->registers.rip = (uint64_t)task->signal_actions[i].sa_handler;
                
                task->registers.CS = (0x18 | 0x3);
                task->registers.SS = (0x20 | 0x3);

                // Swap to the userspace tables
                task->registers.cr3 = virtual_to_physical((uint64_t)task->ptm->PML4);

                task->signal_count++;

                signal_set &= ~(1UL << i);
                task->kernel_signals &= ~(1UL << i);

                // Mask the signal
                if ((task->signal_actions[i].sa_flags & SA_NODEFER) == 0) {
                    task->signal_mask |= (1UL << i);
                }

                if (task->task_state == BLOCKED) task->woke_by_signal = true;
                break;
            }

            signal_set &= ~(1UL << i);
            task->kernel_signals &= ~(1UL << i);
            if (signal_set == 0) break;

        }

        task->pending_signals = signal_set;
    }
    
    void _run_task(task_t* task, cpu_local_data* local){
        local->current_task = task;
        set_tss_rsp0(task->kernel_stack_top); // :)
        
        if (!task->executing_syscall) _handle_signals(task);

        // Run the task
        if (!task->has_run){
            task->registers.rip = (uint64_t)task->entry;
            
            // Set the rsp
            if (task->registers.rsp == 0) task->registers.rsp = task->user_stack_top;

            if (!task->registers.rflags) task->registers.rflags = 0x202; // Interrupts Enable
            task->has_run = true;

            // Set CS/SS
            if (task->userspace){
                task->registers.CS = (0x18 | 0x3);
                task->registers.SS = (0x20 | 0x3);
            }else{
                task->registers.CS = 0x08;
                task->registers.SS = 0x10;
            }

            task->registers.cr3 = task->ptm ? virtual_to_physical((uint64_t)task->ptm->PML4) : global_ptm_cr3;
        }
        
        local->kernel_stack_top = task->syscall_stack_top;
        local->user_stack_scratch = task->current_user_stack;
        local->userspace_return_address = task->userspace_return_address;
        write_msr(IA32_FS_BASE, task->fs_pointer);
        restore_fpu_state(task->saved_fpu_state);
        _execute_task(&task->registers);
    }

    bool _should_unblock(task_t* task){
        if (task->task_state != BLOCKED) return false;
        if (task->pending_signals) return true;

        if (task->block_type == MEMORY_NON_ZERO){
            uint8_t buffer = 0;
            task->read_memory((void*)task->block_context, &buffer, sizeof(uint8_t));
            
            if (buffer != 0) return true;
            return false;
        }

        return false;
    }
    // @warning Assumes the lock is held!!!
    task_t* _get_free_task(int cpu_id){
        for (task_t* current = task_list; current != nullptr; current = current->next){
            if (!current->is_ready) continue;
            
            /* Check if the task is able to be run (e.g. not blocked, not running in another cpu and counter > 0 and affinity allows it) */
            if (current->task_state == RUNNING || current->task_state == BLOCKED ||
                current->task_state == ZOMBIE ||
                current->counter == 0 || (current->affinity & (1UL << cpu_id)) == 0) continue;

            /* If its able to be run, return it */
            return current;
        }

        // No runnable task found
        return nullptr;
    }

    // @warning Assumes the lock is held!!!
    void _reset_counters(int cpu_id){
        // check if it can run on this (affinity), so we don't constantly reset other tasks and drown other cpus

        for (task_t* current = task_list; current != nullptr; current = current->next){
            // Check if it should be reset / Unblocked. (If its running, skip it)
            if (current->task_state == RUNNING || (current->affinity & (1UL << cpu_id)) == 0) continue;

            // Check if scheduled time has passed
            if (current->schedule_until != 0 && current->schedule_until < local_apic_list->tick_count){
                // If yes reset its state
                current->schedule_until = 0;
                current->task_state = current->previous_state == RUNNING ? PAUSED : current->previous_state;
            }

            // Check if it should be unblocked
            if (current->task_state == BLOCKED && _should_unblock(current)) {
                current->task_state = PAUSED;
            }

            current->counter = TASK_SCHED_DEFAULT_COUNT;
        } 
    }

    // Wake up any blocked task waiting on that block_type
    void wake_blocked_tasks(task_block_type_t block_type){
        uint64_t rflags = spin_lock(&task_list_lock);
        
        for (task_t* current = task_list; current != nullptr; current = current->next){
            if (current->task_state == BLOCKED && current->block_type == block_type)
                current->task_state = PAUSED;
        }
    
        spin_unlock(&task_list_lock, rflags);
    }


    // @brief It will save the state (registers) and transition its state to PAUSED (if it was running, otherwise state is left unchanged)
    void _save_task(task_t* task, __registers_t* regs, cpu_local_data* local){
        if (!task) return;

        save_fpu_state(task->saved_fpu_state);

        memcpy(&task->registers, regs, sizeof(__registers_t));
        task->task_state = task->task_state == RUNNING ? PAUSED : task->task_state;
        task->current_user_stack = local->user_stack_scratch;
        task->userspace_return_address = local->userspace_return_address;
    }

    // @brief Will try to find tasks to run. If there are none, it will run the idle task
    void _execute_next_task(__registers_t* regs, cpu_local_data* local /* Avoid refetching it */){
        // Ensure interrupts are disabled... This is a critical stage
        asm ("cli");
        asm ("mov %0, %%rsp" :: "r" (local->scheduler_stack));

        
        // Acquire a lock for the task list
        uint64_t flags = spin_lock(&task_list_lock);
        
        // Save the previous task
        _save_task(local->current_task, regs, local);

        // Check if there are any available tasks
        task_t* next = _get_free_task(local->cpu_id);
        
        // If no task was found, reset the counters and try again
        if (next == nullptr){
            _reset_counters(local->cpu_id);
            next = _get_free_task(local->cpu_id);
        }

        // If still no task was found, execute the idle task
        if (next == nullptr) {
            next = local->idle_task;
            local->idle_time++;
        }

        next->task_state = RUNNING;

        spin_unlock(&task_list_lock, flags);
        _run_task(next, local);
    }

    void scheduler_tick(__registers_t* regs, bool change_task){
        cpu_local_data* local = get_cpu_local_data();

        if (!local || local->disable_scheduling) return;

        local->update_tick_count++;
        /*if (local->update_tick_count >= UTILIZATION_UPDATE_FREQUENCY){
            local->time_in_kernel = 0;
            local->time_in_userspace = 0;
        }*/

        if (local->current_task == nullptr || change_task) return _execute_next_task(regs, local);
        
        if (local->current_task->counter > 0) {
            local->current_task->counter--;
            local->current_task->cpu_time++;
        }

        if (local->current_task->userspace){
            local->time_in_userspace++;
        }else {
            if (local->current_task != local->idle_task) local->time_in_kernel++;
        }
        
        if (local->current_task->counter == 0) {
            return _execute_next_task(regs, local);
        }
    }
} // namespace task_scheduler