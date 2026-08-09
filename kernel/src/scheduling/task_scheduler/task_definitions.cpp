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
#include <signum.h>

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

    this->sid = 0;
    this->gid = 0;
    this->uid = 0;

    this->kernel_stack = ((uint64_t)GlobalAllocator.RequestPage()) + PAGE_SIZE;

    if (is_userspace){
        /* Other Stacks Here!!!! */
        vmm = new mm_struct_t();
        this->syscall_stack = ((uint64_t)GlobalAllocator.RequestPage()) + PAGE_SIZE;
        
        int errno = 0;
        this->userspace_stack = (uint64_t)vmm->allocate(DEFAULT_STACK_SIZE, User, errno) + DEFAULT_STACK_SIZE;
    }

    this->saved_fpu_state = GlobalAllocator.RequestPages(DIV_ROUND_UP(g_fpu_storage_size, PAGE_SIZE));
    _init_task_fpu(this);

    this->current_state = NOT_STARTED;

    this->counter = DEFAULT_COUNTER_VALUE;
}

// Here we should prepare for exit... and clean up a couple of stuff!
void task_t::exit(int exit_code){
    this->current_state = ZOMBIE;

    if (task_scheduler::get_current_task() == this){
        task_scheduler::swap_tasks(); // Exit is only called while this is running (by itself)
                                      // But you never know
    }
}

void task_t::block(){
    // Set up the variables
    this->current_state = BLOCKED;
    this->block_list = nullptr;
    this->block_deadline = 0;
    this->block_status = 0;

    // If this task is currently executing, swap to avoid returning prematurely
    if (task_scheduler::get_current_task() == this)
        task_scheduler::swap_tasks();
}

void task_t::block(uint64_t deadline, kstd::avl_tree_t<task_t*> *block_list){
    // Set up the variables
    this->current_state = BLOCKED;
    this->block_list = block_list;
    this->block_deadline = time_since_boot + deadline;
    this->block_status = 0;

    // Add this task to the timed block list
    uint64_t rflags = spin_lock(&task_scheduler::timed_block_list_lock);
    task_scheduler::timed_block_list.insert(this->pid, this);
    spin_unlock(&task_scheduler::timed_block_list_lock, rflags);

    // If this task is currently executing, swap to avoid returning prematurely
    if (task_scheduler::get_current_task() == this)
        task_scheduler::swap_tasks();
}

void task_t::unblock(){
    if (!this->current_state == BLOCKED && !this->current_state == INTERRUPTABLE) return;

    // Remove it from any block list
    if (this->block_deadline){
        task_scheduler::timed_block_list.remove(this->pid);
    }

    if (this->block_list){
        this->block_list->remove(this->pid);
    }

    // Unblock the task
    this->current_state = PAUSED;

    // Insert it again into the scheduling queue
    this->cpu_queue->push(this);
}

int task_t::read_from_userspace(void *kbuffer, void *uaddress, size_t size){
    if (!this->vmm) return -EFAULT;

    uint64_t current_cr3 = 0;
    uint64_t task_cr3 = this->vmm->get_root_page_table();
    asm volatile ("mov %%cr3, %0" : "=r" (current_cr3));

    if (current_cr3 != task_cr3){
        asm volatile ("mov %0, %%cr3" :: "r" (task_cr3));
    }

    memcpy(kbuffer, uaddress, size);

    if (current_cr3 != task_cr3){
        asm volatile ("mov %0, %%cr3" :: "r" (current_cr3));
    }

    return 0;
}

int task_t::write_to_userspace(void *uaddress, void *kbuffer, size_t size){
    if (!this->vmm) return -EFAULT;

    uint64_t current_cr3 = 0;
    uint64_t task_cr3 = this->vmm->get_root_page_table();
    asm volatile ("mov %%cr3, %0" : "=r" (current_cr3));

    if (current_cr3 != task_cr3){
        asm volatile ("mov %0, %%cr3" :: "r" (task_cr3));
    }

    memcpy(uaddress, kbuffer, size);

    if (current_cr3 != task_cr3){
        asm volatile ("mov %0, %%cr3" :: "r" (current_cr3));
    }

    return 0;
}