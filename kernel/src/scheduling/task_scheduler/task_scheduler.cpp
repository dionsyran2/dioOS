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

    if (this->proc_vfs_dir){
        char buff[25];
        stringf(buff, 25, "%d", num);

        vnode_t* fd_dir = nullptr;
        this->proc_vfs_dir->find_file("fd", &fd_dir);

        if (!fd_dir){
            this->proc_vfs_dir->mkdir("fd");
            this->proc_vfs_dir->find_file("fd", &fd_dir);
        }

        if (fd_dir){
            fd_dir->creat(buff);

            vnode_t* file = nullptr;
            fd_dir->find_file(buff, &file);

            if (file){
                char* str = vfs::get_full_path_name(node);
                file->write(0, strlen(str), str);

                free(str);
                file->close();
            }
            fd_dir->close();
        }
    }

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
    if (this->proc_vfs_dir){
        fd_t* fd = get_fd(num);
        if (fd){
            char buff[25];
            stringf(buff, 25, "%d", fd->num);

            vnode_t* fd_dir = nullptr;
            this->proc_vfs_dir->find_file("fd", &fd_dir);

            if (fd_dir){
                vnode_t* file = nullptr;
                this->proc_vfs_dir->find_file(buff, &file);

                if (file){
                    file->unlink();
                    file->close();
                }

                fd_dir->close();
            }
        }
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
bool task_t::read_memory(void* address, void* buffer, size_t length){
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

// @brief Write to process memory
bool task_t::write_memory(void* address, const void* buffer, size_t length){
    size_t bytes_copied = 0;
    PageTableManager* PTM = this->ptm ? this->ptm : &globalPTM;

    while (bytes_copied < length) {
        uint64_t user_va = (uint64_t)address + bytes_copied;
        uint64_t page_offset = user_va & 0xFFF;
        
        size_t chunk_size = 0x1000 - page_offset;
        size_t remaining = length - bytes_copied;
        if (chunk_size > remaining) chunk_size = remaining;

        uint64_t phys = PTM->getPhysicalAddress((void*)user_va);
        if (phys == 0) return false;
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

#include <drivers/serial/serial.h>
void task_t::exit(int status, bool silent){
    asm ("cli");

    cpu_local_data *local = get_cpu_local_data();
    asm ("mov %0, %%rsp" :: "r" (local->scheduler_stack));

    for (fd_t *fd = this->file_descriptors; fd != nullptr;){
        fd_t* ofd = fd;
        fd = fd->next;
        if (ofd->owner == this) close_fd(ofd->num);
    }

    this->status_code = (status << 8);

    this->task_state = ZOMBIE;
    if (!this->vm_mirror) {
        // You don't want to know how much i was chasing this fucking leak
        GlobalAllocator.FreePages((void*)(this->syscall_stack_top - TASK_STACK_SIZE), DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE));
        GlobalAllocator.FreePages((void*)(this->kernel_stack_top - TASK_STACK_SIZE), DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE));
        
        if (this->userspace){
            // Let the vm_tracker handle it
            this->vm_tracker.change_flags(this->user_stack_top - TASK_STACK_SIZE, TASK_STACK_SIZE, VM_FLAG_RW | VM_FLAG_US);
            // If this->userspace is false, this->user_stack_top is the same as this->kernel_stack_top!
        }
        
        // Clear any memory allocated by the task (does not include the stacks)
        this->vm_tracker.exit(this->ptm, true);
    }

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
    uint64_t current_tgid = 1;

    void mark_ready(task_t* task){
        task->is_ready = true;
    }

    void idle_function(){
        __asm__ ("sti");
        while(1) __asm__ ("hlt");
    }

    task_t* get_process(tid_t pid){
        for (task_t* current = task_list; current != nullptr; current = current->next){
            if (current->pid == pid) return current;
        }

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
        for (task_t* c = task_list; c != nullptr; c = c->next){
            if (c->pgid == thread->pgid && c != thread && c->task_state != ZOMBIE){
                c->exit(status);
            }
        }

        thread->exit(status);
    }

    // @warning Returns the top of the stack (Allocates a stack)
    uint64_t _allocate_stack(task_t* task, uint64_t stack_top){
        if (!task->ptm) return (uint64_t)GlobalAllocator.RequestPages(DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE)) + TASK_STACK_SIZE;

        uint64_t base = stack_top - TASK_STACK_SIZE;

        
        uint64_t ret = 0;
        for (uint64_t o = 0; o < TASK_STACK_SIZE; o += 0x1000){
            uint64_t allocation = (uint64_t)GlobalAllocator.RequestPage();
            uint64_t physical = virtual_to_physical(allocation);

            task->ptm->MapMemory((void*)(base + o), (void*)physical);
            task->ptm->SetFlag((void*)(base + o), PT_Flag::User, true);
            task->vm_tracker.mark_allocation(base + o, PAGE_SIZE, VM_FLAG_COW | VM_FLAG_RW | VM_FLAG_US | VM_FLAG_DONT_FREE);

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
        // Default value: 0x1F80 (Mask all SIMD exceptions)
        // CRITICAL: If this is 0, the first SSE instruction might crash!
        uint32_t* mxcsr = (uint32_t*)((uint8_t*)task->saved_fpu_state + 24);
        *mxcsr = 0x1F80;

    }

    task_t* _create_task_structure(const char* name, function entry, bool userspace, bool create_ptm, bool init){
        bool intr = are_interrupts_enabled();
        asm ("cli");

        task_t* task = new task_t;
        memset(task, 0, sizeof(task_t));

        task->vm_tracker.lock = 0;
        task->vm_tracker.vm_list = nullptr;
        task->vm_tracker.total_marked_memory = 0;
        task->start_time = GetTicks();

        PageTableManager* ptm = &globalPTM;

        if (create_ptm){
            ptm = new PageTableManager((PageTable*)GlobalAllocator.RequestPage(), &task->vm_tracker);
            task->vm_tracker.mark_allocation((uint64_t)ptm->PML4, PAGE_SIZE, VM_FLAG_RW | VM_FLAG_DO_NOT_SHARE);
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
        if (userspace) {
            task->user_stack_top = _allocate_stack(task, 0x800000000000);
        } else {
            task->user_stack_top = task->kernel_stack_top;
        }
        task->syscall_stack_top = (uint64_t)GlobalAllocator.RequestPages(DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE)) + TASK_STACK_SIZE;

        task->affinity = UINT64_MAX; // Enable all cores

        if (!init){
            task->pid = __atomic_fetch_add(&current_pid, 1, __ATOMIC_SEQ_CST);
            task->tgid = __atomic_fetch_add(&current_tgid, 1, __ATOMIC_SEQ_CST);
            task->pgid = task->pid;
        } else {
            task->pid = task->tgid = task->pgid = 1;
        }

        vnode_t *root = vfs::resolve_path("/");
        root->open();
        task->cwd = task->open_node(root, 0x99);

        _init_task_fpu(task);
        _add_task_to_list(task);

        if (intr) asm ("sti");

        return task;
    }

    task_t* create_process(const char* name, function entry, bool userspace, bool ptm, bool init){
        task_t* task = _create_task_structure(name, entry, userspace, ptm, init);

        if (userspace){
            vnode_t* proc = vfs::resolve_path("/proc");
            
            if (proc){
                proc->mkdir(toString((uint64_t)task->pid));
                char full_path[256];
                stringf(full_path, sizeof(full_path), "/proc/%d", task->pid);
                task->proc_vfs_dir = vfs::resolve_path(full_path);
                task->proc_vfs_dir->uid = task->ruid;
                task->proc_vfs_dir->gid = task->rgid;

                _add_dynamic_task_virtual_files(task);
            }
        }

        return task;
    }

    task_t* fork_process(task_t* process, bool share_vm, bool share_files){
        task_t* task = _create_task_structure(process->name, nullptr, true, false, false);

        memcpy(task->saved_fpu_state, process->saved_fpu_state, g_fpu_storage_size);

        if (!share_vm){
            task->ptm = new PageTableManager((PageTable*)GlobalAllocator.RequestPage(), &task->vm_tracker);
            memset(task->ptm->PML4, 0, PAGE_SIZE);
            ClonePTM(task->ptm, process->ptm);
        } else {
            task->ptm = process->ptm;
            task->vm_mirror = true;
        }

        task->brk_offset = process->brk_offset;
        task->rmap_offset = process->rmap_offset;


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

        task->vm_tracker.lock = 0;
        task->vm_tracker.vm_list = nullptr;
        task->vm_tracker.total_marked_memory = 0;
        task->start_time = GetTicks();
        
        PageTableManager* ptm = new PageTableManager((PageTable*)GlobalAllocator.RequestPage(), &task->vm_tracker);
        task->vm_tracker.mark_allocation((uint64_t)ptm->PML4, PAGE_SIZE, VM_FLAG_RW | VM_FLAG_DO_NOT_SHARE);
        memset(ptm->PML4, 0, PAGE_SIZE);
        ClonePTM(ptm, &globalPTM);

        memcpy(task->name, victim->name, strlen(victim->name));


        task->ptm = ptm;
        task->userspace = victim->userspace;
        task->is_krnl = victim->is_krnl;
        task->is_ready = false;

        task->counter = TASK_SCHED_DEFAULT_COUNT;

        task->kernel_stack_top = (uint64_t)GlobalAllocator.RequestPages(DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE)) + TASK_STACK_SIZE;
        if (task->userspace) {
            task->user_stack_top = _allocate_stack(task, 0x800000000000);
        } else {
            task->user_stack_top = task->kernel_stack_top;
        }
        task->syscall_stack_top = (uint64_t)GlobalAllocator.RequestPages(DIV_ROUND_UP(TASK_STACK_SIZE, PAGE_SIZE)) + TASK_STACK_SIZE;

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

        vnode_t* proc = vfs::resolve_path("/proc");
        
        if (proc){
            proc->mkdir(toString((uint64_t)task->pid));
            char full_path[256];
            stringf(full_path, sizeof(full_path), "/proc/%d", task->pid);
            task->proc_vfs_dir = vfs::resolve_path(full_path);
            _add_dynamic_task_virtual_files(task);

            task->proc_vfs_dir->uid = task->ruid;
            task->proc_vfs_dir->gid = task->rgid;
        }

        for (fd_t* fd = victim->file_descriptors; fd != nullptr; fd = fd->next){
            if (fd->flags & O_CLOEXEC) continue;

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
            root->open();

            task->cwd = task->open_node(root, 0x99);
        }
        
        _init_task_fpu(task);
        _add_task_to_list(task);

        if (intr) asm ("sti");

        return task;
    }
    
    void _run_task(task_t* task, cpu_local_data* local){
        local->current_task = task;
        set_tss_rsp0(task->kernel_stack_top); // :)

        // Run the task
        if (!task->has_run){
            task->registers.rip = (uint64_t)task->entry;
            
            // Set the rsp
            if (task->registers.rsp == 0) task->registers.rsp = task->user_stack_top;

            task->registers.rflags = 0x202; // Interrupts Enable
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
            if (current->task_state == BLOCKED && _should_unblock(current)) current->task_state = PAUSED;

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
