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
#include <memory/heap.h>
#include <panic.h>
#include <structures/trees/avl_tree.h>
#include <structures/lists/linked_list.h>
#include <kerrno.h>

extern "C" [[noreturn]] void _execute_task(__registers_t *); // Defined in task_scheduler.asm

namespace task_scheduler {
    kstd::avl_tree_t<task_t *> task_search_tree;
    kstd::avl_tree_t<task_t *> timed_block_list;
    kstd::linked_list_t<task_t *> pending_unblock;
    kstd::linked_list_t<task_t *> pending_cleanup;

    task_t *cleanup_task = nullptr;

    spinlock_t timed_block_list_lock = 0;
    uint64_t last_block_list_check;

    kstd::linked_list_t<cpu_task_queue_t *> cpu_task_queues;

    pid_t current_pid = 1000;


    // The idle method
    void idle(){
        while (1) {
            asm ("sti; hlt"); // Just halt to reduce cpu power
        }
    }

    // Forward declaration
    task_t *create_process(const char *name, function entry, bool userspace, bool init);
    void task_cleaner();

    // Initialize the scheduler for the local core
    void initialize_core(){
        // Get a reference to the local core data
        cpu_local_data *local = get_cpu_local_data();

        // Allocate a scheduler stack (Used while swapping tasks)
        local->scheduler_stack = ((uint64_t)GlobalAllocator.RequestPage()) + PAGE_SIZE;

        // Create the cpu task queue
        local->scheduler_queue = new cpu_task_queue_t(TASK_QUEUE_SIZE);

        // Add it to the list for the queues
        cpu_task_queues.lock();
        cpu_task_queues.add(local->scheduler_queue);
        cpu_task_queues.unlock();

        // Create the idle task!!!!!!!
        local->idle_task = create_process("sched_idle", idle, false);

        if (cleanup_task == nullptr) {
            cleanup_task = create_process("sched_cleanup_thread", task_cleaner, false, false);
            mark_as_ready(cleanup_task);
        }
    }


    void mark_as_ready(task_t *task){
        // This used to change a boolean value, but since we moved to a per cpu queue design
        // We are just gonna find the queue with the least amount of tasks and insert it there
        // that should in theory keep everything balanced
        
        cpu_task_queue_t *queue = nullptr;
        
        cpu_task_queues.lock(); // acquire the lock... even though it shouldn't be modified

        for (int i = 0; i < cpu_task_queues.size(); i++){
            cpu_task_queue_t *q = cpu_task_queues.get(i);

            // If the queue variable is empty or we found a queue with
            // less tasks, replace it
            if (!queue || q->entry_count < queue->entry_count){
                queue = q;
            }
        }

        cpu_task_queues.unlock(); // We have to unlock it

        if (!queue) {
            // Well, I have no idea how this could happen, but better catch it if it does!
            panic("Scheduler fault!\n mark_as_ready() was called on a task but no scheduler queues were found!");
        }

        // Add the task to that cpu's queue.
        queue->push(task);
    }

    task_t *get_current_task(){
        cpu_local_data *local = get_cpu_local_data();
        
        return local->current_task;
    }

    task_t *search_by_pid(pid_t pid){
        return task_search_tree.search(pid);
    }

    task_t *create_process(const char *name, function entry, bool userspace, bool init){
        pid_t pid = init ? 1 : __atomic_fetch_add(&current_pid, 1, __ATOMIC_SEQ_CST);
        task_t *r = new task_t(entry, pid, pid, userspace);

        strncpy(r->name, name, sizeof(r->name));
        task_search_tree.insert(pid, r);

        return r;
    }

    task_t *clone(uint64_t flags, uint64_t rsp, __registers_t *registers){
        task_t *parent = task_scheduler::get_current_task();
        
        task_t *child = create_process(parent->name, (function)registers->rip, true, false);
        child->ppid = parent->pid;

        memcpy(child->saved_fpu_state, parent->saved_fpu_state, g_fpu_storage_size);

        child->fd_table->close();
        if (flags & CLONE_FILES) {
            child->fd_table = parent->fd_table;
            parent->fd_table->open();
        } else {
            child->fd_table = parent->fd_table->clone();
        }

        if (flags & CLONE_VM) {
            child->vmm = parent->vmm;
            child->vmm->open();
        } else {
            child->vmm = parent->vmm->fork();
        }

        memcpy(&child->supplementary_groups, parent->supplementary_groups, sizeof(parent->supplementary_groups));
        child->num_supplementary_groups = parent->num_supplementary_groups;
        
        memcpy(&child->registers, registers, sizeof(__registers_t));
        child->registers.rax = 0;
        child->fs_pointer = parent->fs_pointer;

        if (rsp != 0) {
            child->registers.rsp = rsp;
        }

        return child;
    }

    // Forcibly causes a scheduler tick & swaps task
    void swap_tasks(){
        asm ("sti; int %0" :: "i" (SCHEDULER_SWAP_TASKS_VECTOR));
    }

    [[noreturn]] void __run_task(task_t *task){
        if (!task->is_executing_syscall) task->check_pending_signals();
        
        cpu_local_data* local = get_cpu_local_data();

        bool has_run = task->current_state != NOT_STARTED;

        // Set the task state
        task->current_state = RUNNING;

        // Set the current task to resemble this one we are about to execute
        local->current_task = task;

        // Set the rsp on transition from ring3->ring0
        set_tss_rsp0(task->kernel_stack);

        if (!has_run){
            // Since it hasn't been started yet, we need to configure its registers

            // Set the rip:
            if (task->registers.rip == 0) task->registers.rip = (uint64_t)task->task_entry_point;

            // Set the stack. For userspace tasks this will be the userspace stack, otherwise
            // it should be the kernel stack!
            if (task->registers.rsp == 0) {
                task->registers.rsp = task->is_userspace ? task->userspace_stack : task->kernel_stack;
            }


            // Set the rflags
            if (!task->registers.rflags) task->registers.rflags = 0x202;

            // Set the CodeSegment and StackSegment values according to the ring
            if (task->is_userspace){
                task->registers.CS = (0x18 | 0x3);
                task->registers.SS = (0x20 | 0x3);
            }else{
                task->registers.CS = 0x08;
                task->registers.SS = 0x10;
            }

            // And of course the page tables
            task->registers.cr3 = task->vmm ? task->vmm->get_root_page_table() : global_ptm_cr3;
        }

        local->kernel_stack_top = task->kernel_stack;
        local->user_stack_scratch = task->current_user_stack;
        local->userspace_return_address = task->userspace_return_address;
        write_msr(IA32_FS_BASE, task->fs_pointer);
        restore_fpu_state(task->saved_fpu_state);

        task->quantum_start = TSC::get_uptime_ns() / 1000;

        _execute_task(&task->registers);
    }

    void __save_task(__registers_t *regs, task_t *task){
        cpu_local_data* local = get_cpu_local_data();

        // save the current fpu state
        save_fpu_state(task->saved_fpu_state);
        
        // save the register state
        memcpy(&task->registers, regs, sizeof(__registers_t));

        // These are used to save the return stack/address for the syscall handler
        task->current_user_stack = local->user_stack_scratch;
        task->userspace_return_address = local->userspace_return_address;

        if (task->current_state == RUNNING){
            task->current_state = PAUSED;
        }
        
        // Restore its counter
        task->counter = DEFAULT_COUNTER_VALUE;
    }

    task_t *__steal_tasks(){
        cpu_local_data* local = get_cpu_local_data();

        cpu_task_queue_t *queue = nullptr;

        // Try to find the queue with the most tasks
        cpu_task_queues.lock(); // acquire the lock

        for (int i = 0; i < cpu_task_queues.size(); i++){
            cpu_task_queue_t *q = cpu_task_queues.get(i);

            if (q == local->scheduler_queue) continue; // Skip our own queue

            // If the queue variable is empty or we found a queue with
            // more tasks, replace it
            if (!queue || q->entry_count > queue->entry_count){
                queue = q;
            }
        }

        cpu_task_queues.unlock(); // We have to unlock it

        // This will steal from the victim (queue) half of its tasks, 'pop' the first task and return it
        if (queue) return local->scheduler_queue->steal(queue); 
        
        return nullptr;
    }

    task_t *__find_runnable_task(){
        cpu_local_data* local = get_cpu_local_data();

        // Check if we have any tasks in out queue
        task_t *r = local->scheduler_queue->pop();

        // Check if we actually got a task, if yes return it
        if (r) return r;

        // If not, we should try to steal some
        r = __steal_tasks();

        // Check if its a valid task
        if (r && r->current_state == ZOMBIE) return __find_runnable_task(); // Keep this popped and find another one... this one is dead!

        // Check if we stole any, if yes return it
        if (r) return r;

        // If not return the idle task
        return local->idle_task;
    }

    [[noreturn]] void __execute_next_task(__registers_t* regs){
        asm ("cli");

        cpu_local_data* local = get_cpu_local_data();
        
        bool should_push = false;
        // Save the current task's state
        if (local->current_task) {
            if (local->current_task->current_state == RUNNING || local->current_task->current_state == INTERRUPTABLE){
                should_push = true;
            }

            __save_task(regs, local->current_task);

            if (should_push){
                local->scheduler_queue->push(local->current_task);
            }
        }
        
        task_t *next = __find_runnable_task();

        __run_task(next);
    }

    void _sched_block_list_check_cb(task_t *task, void*){
        // We should not unblock
        if (task->block_deadline > time_since_boot) return;
        
        // We should unblock. The task has timed out so we also set the block_status.
        task->block_status = -ETIMEDOUT;
        
        // task->unblock() will be called later on so we don't corrupt the avl tree.
        pending_unblock.add(task);
    }

    void _check_block_list(){
        // It will check for expired blocks in the block list
        uint64_t now = time_since_boot;
        uint64_t expected = last_block_list_check;

        // Fast path: If the sweep has already reached or passed our current millisecond,
        // another core beat us to the punch. Exit immediately without touching it.
        if (now <= expected) {
            return;
        }

        // An atomic exchange operation. If we win we will check the list for this millisecond,
        // otherwise another core beat us to it
        
        if (__atomic_compare_exchange_n(&last_block_list_check, &expected, now, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            uint64_t rflags = spin_lock(&timed_block_list_lock);

            pending_unblock.lock();
            timed_block_list.inorder(_sched_block_list_check_cb, nullptr);

            spin_unlock(&timed_block_list_lock, rflags);
        
            task_t *c = pending_unblock.get(0);

            while (c){
                c->unblock();
                pending_unblock.remove(0);

                c = pending_unblock.get(0);
            }

            pending_unblock.unlock();
        }
    }

    // Called by either the apic timer handler or the change task vector handler.
    void scheduler_tick(__registers_t* regs, bool change_task){
        /* Basically regs are the cpu registers before the interrupt and change_task notes whether
         * this call was caused by the change task vector handler or not.
         */
        cpu_local_data* local = get_cpu_local_data();
        
        // Check the blocked list if this is not a forced interrupt!
        if (!change_task){
            _check_block_list();
        }

        // If the cpu has not been set up yet or scheduling is disabled return...
        // we don't want to cause a fault
        if (!local || local->disable_scheduling) return;

        // If we are running a task, decrease its count
        if (local->current_task){
            local->current_task->counter--;

            uint64_t quantum = (TSC::get_uptime_ns() / 1000) - local->current_task->quantum_start;
            // If the cpu was not idle, update its time counters
            // (I hate nested if statements but this is the cleanest way)
            if (local->current_task != local->idle_task){
                if (local->current_task->is_userspace){
                    local->time_in_userspace += quantum;
                } else {
                    local->time_in_kernel += quantum;
                }
            } else {
                // If we run the idle task, increase the respective timer
                local->idle_time += quantum;
            }
        }

        // Now check if we should swap tasks
        // (We should swap if we are not executing a task, the change_task variable is set
        //  Or if the counter of the current_task reaches zero.)
        if (!local->current_task || change_task || local->current_task->counter == 0){
            // Because we have optimisations turned off and the way this code compiles
            // it should be safe to swap to out scheduler task to avoid other cpus trying to 
            // execute the current_task (In case a core steals it before we execute the next one)
            // So before we save it we should swap. Since the methods never return it should be fine
            __asm__ __volatile__ (
                "mov %1, %%rdi\n\t"
                "mov %0, %%rsp\n\t"
                "jmp *%2\n\t"
                :
                : "r" (local->scheduler_stack), "r" (regs), "r" (__execute_next_task)
                : "rdi", "memory"
            );

            
            __builtin_unreachable();
        }
    }
}