#include <scheduling/task_scheduler/task_scheduler.h>
#include <paging/PageFrameAllocator.h>
#include <interrupts/interrupts.h>
#include <memory.h>
#include <memory/heap.h>
#include <math.h>
#include <cstr.h>
#include <local.h>
#include <drivers/timers/common.h>
#include <drivers/serial/serial.h>
#include <structures/trees/avl_tree.h>
#include <kerrno.h>
#include <vfs/vnode.h>

// Initializes the fpu state buffer
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

task_t::task_t(function entry, pid_t pid, tid_t tgid, bool is_userspace){
    this->task_entry_point = entry;
    this->pid = pid;
    this->tgid = tgid;
    this->is_userspace = is_userspace;

    this->kernel_stack = ((uint64_t)GlobalAllocator.RequestPage()) + PAGE_SIZE;

    if (is_userspace){
        /* Other Stacks Here!!!! */
        vmm = new mm_struct_t();
        this->syscall_stack = ((uint64_t)GlobalAllocator.RequestPage()) + PAGE_SIZE;
        
        int errno = 0;
        this->userspace_stack = (uint64_t)vmm->mmap(0x7FFFFFFF000 - DEFAULT_STACK_SIZE, DEFAULT_STACK_SIZE, VM_WRITE, nullptr, 0, errno) + DEFAULT_STACK_SIZE;

        this->fd_table = new fd_table_t();
        this->fd_table->open();
        this->fd_table->cwd = vfs::resolve_path_dentry("/");
    }

    _init_task_fpu(this);

    this->current_state = NOT_STARTED;

    this->counter = DEFAULT_COUNTER_VALUE;
}

namespace task_scheduler {
    extern kstd::avl_tree_t<task_t *> task_search_tree;
    extern kstd::linked_list_t<task_t *> pending_cleanup;
    extern task_t *cleanup_task;
}

// Here we should prepare for exit... and clean up a couple of stuff!
void task_t::exit(int exit_code){
    asm ("cli");
    this->current_state = ZOMBIE;

    if (this->clear_child_tid != nullptr) {
        
        int zero = 0;
        
        this->write_to_userspace(this->clear_child_tid, &zero, sizeof(int));
        
        //futex_wake_address(this->clear_child_tid, 1);
    }

    this->exit_code = exit_code;
    
    serialf("%d exit %d\n\r", this->pid, exit_code);

    task_scheduler::pending_cleanup.lock();
    task_scheduler::pending_cleanup.add(this);
    task_scheduler::pending_cleanup.unlock();

    task_scheduler::cleanup_task->unblock();
    
    asm ("sti");
    if (task_scheduler::get_current_task() == this){
        task_scheduler::swap_tasks(); // Exit is only called while this is running (by itself)
                                      // But you never know
    }
}

void task_t::block(){
    // Set up the variables
    this->blocking_lock.lock();
    this->current_state = BLOCKED;
    this->block_list = nullptr;
    this->block_deadline = 0;
    this->block_status = 0;

    this->blocking_lock.unlock();
    // If this task is currently executing, swap to avoid returning prematurely
    if (task_scheduler::get_current_task() == this)
        task_scheduler::swap_tasks();
}

void task_t::block(uint64_t deadline, kstd::avl_tree_t<task_t*> *block_list){
    // Set up the variables
    this->blocking_lock.lock();
    this->current_state = BLOCKED;
    this->block_list = block_list;
    this->block_deadline = time_since_boot + deadline;
    this->block_status = 0;

    // Add this task to the timed block list
    uint64_t rflags = spin_lock(&task_scheduler::timed_block_list_lock);
    task_scheduler::timed_block_list.insert(this->pid, this);
    spin_unlock(&task_scheduler::timed_block_list_lock, rflags);
    this->blocking_lock.unlock();

    // If this task is currently executing, swap to avoid returning prematurely
    if (task_scheduler::get_current_task() == this)
        task_scheduler::swap_tasks();
}

void task_t::unblock(){
    // Record if it was actually asleep before we modify the state
    this->blocking_lock.lock();
    bool was_blocked = (this->current_state == BLOCKED);

    if (this->current_state != BLOCKED && this->current_state != INTERRUPTABLE) {
        this->blocking_lock.unlock();
        return;
    }

    // Remove it from any block list
    if (this->block_deadline){
        task_scheduler::timed_block_list.remove(this->pid);
    }

    if (this->block_list){
        this->block_list->remove(this->pid);
    }

    // Unblock the task
    this->current_state = PAUSED;

    if (was_blocked) {
        this->cpu_queue->push(this);
    }

    this->blocking_lock.unlock();
}

int task_t::read_from_userspace(void *kbuffer, const void *uaddress, size_t size){
    if (!this->vmm) return -EFAULT;
    if (size == 0) return 0;

    uintptr_t start = (uintptr_t)uaddress;
    uintptr_t end = start + size;

    uint64_t current_cr3 = 0;
    uint64_t task_cr3 = this->vmm->get_root_page_table();
    asm volatile ("mov %%cr3, %0" : "=r" (current_cr3));
    if (current_cr3 != task_cr3) {
        asm volatile ("mov %0, %%cr3" :: "r" (task_cr3));
    }

    uintptr_t curr_page = ALIGN_DOWN(start, PAGE_SIZE);
    while (curr_page < end) {
        uint64_t phys = this->vmm->resolve_physical_address(curr_page);
        
        if (phys == 0) {
            bool success = this->vmm->handle_page_fault(curr_page, 0x0, true);
            if (!success) {
                if (current_cr3 != task_cr3) asm volatile ("mov %0, %%cr3" :: "r" (current_cr3));
                return -EFAULT;
            }
        }
        curr_page += PAGE_SIZE;
    }

    memcpy(kbuffer, uaddress, size);

    if (current_cr3 != task_cr3){
        asm volatile ("mov %0, %%cr3" :: "r" (current_cr3));
    }

    return 0;
}

int task_t::write_to_userspace(void *uaddress, const void *kbuffer, size_t size){
    if (!this->vmm) return -EFAULT;
    if (size == 0) return 0;

    uint64_t start = (uint64_t)uaddress;
    uint64_t end = start + size;

    uint64_t current_cr3 = 0;
    uint64_t task_cr3 = this->vmm->get_root_page_table();
    asm volatile ("mov %%cr3, %0" : "=r" (current_cr3));
    if (current_cr3 != task_cr3) {
        asm volatile ("mov %0, %%cr3" :: "r" (task_cr3));
    }

    uint64_t curr_page = ALIGN_DOWN(start, PAGE_SIZE);
    while (curr_page < end) {
        __m_area_t *vma = this->vmm->find_vma(curr_page);
        if (!vma || !(vma->flags & VM_WRITE)) {
            if (current_cr3 != task_cr3) asm volatile ("mov %0, %%cr3" :: "r" (current_cr3));
            return -EFAULT; // Standard POSIX response for a bad/RO pointer
        }

        uint64_t phys = this->vmm->resolve_physical_address(curr_page);
        bool hw_writable = this->vmm->_page_table_manager->GetFlag((void*)curr_page, PT_Flag::Write);

        // If it's not mapped (Demand Paging) OR mapped but Read-Only (CoW)
        if (phys == 0 || !hw_writable) {
            
            bool success = this->vmm->handle_page_fault(curr_page, 0x2, false); 
            
            if (!success) {
                if (current_cr3 != task_cr3) asm volatile ("mov %0, %%cr3" :: "r" (current_cr3));
                return -EFAULT;
            }
        }
        curr_page += PAGE_SIZE;
    }

    // Now that all pages are guaranteed to be mapped AND writable in hardware, safe to copy!
    memcpy(uaddress, kbuffer, size);

    if (current_cr3 != task_cr3){
        asm volatile ("mov %0, %%cr3" :: "r" (current_cr3));
    }

    return 0;
}

char *task_t::read_string(const char *uaddress){
    if (!this->vmm) return nullptr;

    uint64_t current_cr3 = 0;
    uint64_t task_cr3 = this->vmm->get_root_page_table();
    asm volatile ("mov %%cr3, %0" : "=r" (current_cr3));
    if (current_cr3 != task_cr3) {
        asm volatile ("mov %0, %%cr3" :: "r" (task_cr3));
    }

    size_t length = 0;
    bool found_null = false;
    const char *p = uaddress;

    while (length < 1024) {
        uintptr_t page = ALIGN_DOWN((uintptr_t)p, PAGE_SIZE);
        uint64_t phys = this->vmm->resolve_physical_address(page);
        
        if (phys == 0) {
            bool success = this->vmm->handle_page_fault(page, 0x0, true);
            if (!success) {
                if (current_cr3 != task_cr3) asm volatile ("mov %0, %%cr3" :: "r" (current_cr3));
                return nullptr;
            }
        }

        if (*p == '\0') {
            found_null = true;
            break;
        }

        p++;
        length++;
    }

    if (!found_null) {
        if (current_cr3 != task_cr3) asm volatile ("mov %0, %%cr3" :: "r" (current_cr3));
        return nullptr;
    }

    char *buffer = (char *)malloc(length + 1);
    if (!buffer) {
        if (current_cr3 != task_cr3) asm volatile ("mov %0, %%cr3" :: "r" (current_cr3));
        return nullptr;
    }

    memcpy(buffer, uaddress, length);
    buffer[length] = '\0';

    if (current_cr3 != task_cr3){
        asm volatile ("mov %0, %%cr3" :: "r" (current_cr3));
    }

    return buffer;
}

bool task_t::check_pending_signals() {
    uint64_t deliverable = this->pending_signals & (~this->blocked_signals);
    if (!deliverable) return false;

    int signum = __builtin_ffsll(deliverable) -1;

    this->pending_signals &= ~(1UL << signum);

    this->deliver_signal(signum);
    return true;
}


#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

// Clear any zombie children if configured to do so
void sigcld_avl_cb(task_t *task, void *ctx){
    int ppid = (uint64_t)ctx;

    if (ppid == task->ppid && task->current_state == ZOMBIE){
        uint64_t rflags = spin_lock(&task->cleanup_lock); // Ensure the cleanup task finished
        set_cpu_flags(rflags);
        
        task_scheduler::task_search_tree.remove(task->pid);
        delete task;
    }
}

void task_t::deliver_signal(int signum) {
    sigaction action = this->signal_actions[signum];
    
    // If the signum is sigchld, and sa_nocldwait is set, delete the task
    if (signum == SIGCHLD && (action.sa_flags & SA_NOCLDWAIT)){
        task_scheduler::task_search_tree.inorder(sigcld_avl_cb, (void*)this->pid);
    }

    if (action.sa_handler == SIG_IGN) return;
    if (action.sa_handler == SIG_DFL) {
        if (signum == SIGCHLD || signum == SIGURG) return; // DFLT IGNORE

        // Add termination/core-dump/stop/continue
        this->exit(-signum); // Does not return
    }
    // Calculate a new rsp
    uint64_t user_rsp = this->registers.rsp - 128; // start beyond the red zone
    uint64_t rsi = this->registers.rsi;

    if (action.sa_flags & SA_SIGINFO){
        siginfo_t siginfo = {
            .si_signo = signum,
            /* ... */  
        };

        user_rsp -= sizeof(siginfo_t);
        rsi = user_rsp;

        this->write_to_userspace((void*)user_rsp, &siginfo, sizeof(siginfo_t));
    }
    
    user_rsp -= sizeof(__registers_t);
    user_rsp &= ~0xFUL;

    // Push the old register state
    this->write_to_userspace((void*)user_rsp, &this->registers, sizeof(__registers_t));

    // Push the sa_restorer address
    if (action.sa_restorer) {
        user_rsp -= sizeof(uint64_t);
        this->write_to_userspace((void*)user_rsp, &action.sa_restorer, sizeof(uint64_t));
    }

    this->registers.rdi = signum;
    this->registers.rsi = rsi;
    this->registers.rsp = user_rsp;
    this->registers.rip = (uint64_t)action.sa_handler;
}

namespace task_scheduler {
    extern void __run_task(task_t *task);
}


void task_t::restore_signal(){
    uint64_t saved_state = this->registers.rsp;
    __registers_t regs;

    this->read_from_userspace(&regs, (void*)saved_state, sizeof(__registers_t));

    this->registers = regs;

    task_scheduler::__run_task(this);
}

void task_t::signal(int signum) {
    this->pending_signals |= (1UL << signum);
    if (this->current_state == BLOCKED || this->current_state == INTERRUPTABLE) {
        this->block_status = -EINTR;
        block_intr_info = signum;
        this->unblock();
    }
}

void task_t::signal(int signum, pid_t source) {
    if (this->pending_signals & (1UL << signum)) return; // already pending

    this->signal_source[signum] = source;
    signal(signum);
}