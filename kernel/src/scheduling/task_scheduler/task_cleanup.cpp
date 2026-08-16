#include <scheduling/task_scheduler/task_scheduler.h>
#include <paging/PageFrameAllocator.h>
#include <bits/signals.h>
#include <kstdio.h>
namespace task_scheduler {
    extern kstd::linked_list_t<task_t *> pending_cleanup;
    extern kstd::avl_tree_t<task_t *> task_search_tree;
    
    void avl_cb(task_t *task, void *ctx){
        int ppid = (uint64_t)ctx;

        if (ppid == task->ppid){
            task->ppid = 1;
        }
    }

    void task_cleaner() {
        task_t *self = task_scheduler::get_current_task();
        while (1) {
            if (pending_cleanup.size() == 0) self->block();

            pending_cleanup.lock();
            while (pending_cleanup.size()){
                task_t *victim = pending_cleanup.get(0);
                pending_cleanup.remove(0);
                
                uint64_t rflags = spin_lock(&victim->cleanup_lock);

                //if (victim->vmm) victim->vmm->close();
                if (victim->fd_table) victim->fd_table->close();

                uint64_t required_pages = DIV_ROUND_UP(g_fpu_storage_size, PAGE_SIZE);
                GlobalAllocator.FreePages(victim->saved_fpu_state, required_pages); 

                // Notify the parent
                if (victim->ppid){
                    task_t *parent = task_search_tree.search(victim->ppid);
                    if (parent) {
                        kprintf("PPID: %p\n", victim->ppid);
                        parent->signal(SIGCHLD);
                    }
                }

                // Check if there are any orphaned children and reassign them
                task_search_tree.inorder(avl_cb, (void*)victim->pid);

                bool user = victim->is_userspace;

                GlobalAllocator.FreePage((void*)(victim->kernel_stack - PAGE_SIZE));

                if (!user){
                    task_scheduler::task_search_tree.remove(victim->pid);
                    delete victim;
                    set_cpu_flags(rflags);
                } else {
                    GlobalAllocator.FreePage((void*)(victim->syscall_stack - PAGE_SIZE));
                    spin_unlock(&victim->cleanup_lock, rflags);
                }
            }

            pending_cleanup.unlock();
        }
    }
}