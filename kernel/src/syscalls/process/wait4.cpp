#include <syscalls/syscalls.h>

#define	WNOHANG		1	/* Don't block waiting.  */
#define	WUNTRACED	2	/* Report status of stopped children.  */


long sys_wait4(int pid, int *wstatus, int options, struct rusage *rusage) {
    task_t* self = task_scheduler::get_current_task();
    task_t* child = nullptr;

    while (true) {
        bool found_any_child = false;
        bool found_zombie = false;

        uint64_t rflags = spin_lock(&task_scheduler::task_list_lock);

        for (task_t* t = task_scheduler::task_list; t != nullptr; t = t->next) {
            if (t == self) continue;

            bool is_child = (t->ppid == self->pid);

            if (!is_child) continue;

            found_any_child = true;

            bool match = false;
            if (pid == -1)
                match = true;
            else if (pid > 0)
                match = (t->pid == pid);
            else if (pid == 0)
                match = (t->pgid == self->pgid);
            else if (pid < -1)
                match = (t->pgid == -pid);

            if (!match) continue;

            if (t->task_state == ZOMBIE) {
                found_zombie = true;
                child = t;
                break;
            }
        }

        spin_unlock(&task_scheduler::task_list_lock, rflags);
        
        if (!found_any_child)
            return -ECHILD;

        if (found_zombie)
            break;

        if (options & WNOHANG)
            return 0;

        self->Block(WAIT_FOR_CHILD, pid);

        if (pid == -1){
            for (task_t* t = task_scheduler::task_list; t != nullptr; t = t->next){
                if (t == self) continue;

                bool is_child = (t->ppid == self->pid);
                if (!is_child) continue;
                if (t->task_state == ZOMBIE){
                    child = t;
                    continue;
                }
            }
        }
    }

    int ret = child->pid;

    if (wstatus)
        self->write_memory(wstatus, &child->status_code, sizeof(int));

    task_scheduler::_remove_task_from_list(child);
    delete child;
    return ret;
}

REGISTER_SYSCALL(SYS_wait4, sys_wait4);