#include <syscalls/syscalls.h>

#define WNOHANG         1       /* Don't block waiting.  */
#define WUNTRACED       2       /* Report status of stopped children.  */


namespace task_scheduler {
    extern kstd::avl_tree_t<task_t *> task_search_tree;
    extern task_t *cleanup_task;
}

struct wait4_ctx {
    bool found_any_child;
    bool found_zombie;
    int pid;
    task_t *child;
};

int wait4_avl_cb(task_t *task, void *c){
    task_t *self = task_scheduler::get_current_task();
    wait4_ctx *ctx = (wait4_ctx*)c;
    int pid = ctx->pid;

    if (self == task) return AVL_REC_CONT;

    bool is_child = (task->ppid == self->pid);

    if (!is_child) return AVL_REC_CONT;

    bool match = false;
    if (pid == -1)
        match = true;
    else if (pid > 0)
        match = (task->pid == pid);
    else if (pid == 0)
        match = (task->pgid == self->pgid);
    else if (pid < -1)
        match = (task->pgid == -pid);

    if (!match) return AVL_REC_CONT;
    ctx->found_any_child = true;
    

    if (task->current_state == ZOMBIE) {
        ctx->found_zombie = true;
        ctx->pid = task->pid;
        ctx->child = task;
        return 0; // Will break
    }

    return AVL_REC_CONT;
}


#include <drivers/timers/common.h>

long sys_wait4(int pid, int *wstatus, int options, struct rusage *rusage) {
    task_t* self = task_scheduler::get_current_task();

    wait4_ctx ctx = {
        .found_any_child = false,
        .found_zombie = false,
        .pid = pid
    };

    while (true) {
        self->current_state = INTERRUPTABLE;
        ctx.found_any_child = false;
        ctx.found_zombie = false;

        int result = task_scheduler::task_search_tree.inorder(wait4_avl_cb, &ctx);

        if (!ctx.found_any_child) return -ECHILD;

        if (ctx.found_zombie) break;

        if (options & WNOHANG) return 0;

        if (self->current_state == INTERRUPTABLE) self->block();

        if (self->block_status == -EINTR &&
            self->block_intr_info != SIGCHLD) return -EINTR; // Interrupted by something that is not a zombie
    }

    if (ctx.child) {
        if (wstatus) {
            int ecode = (ctx.child->exit_code & 0xFF) << 8;
            if (self->write_to_userspace(wstatus, &ecode, sizeof(int)) < 0) return -EFAULT;
        }

        // Perform the last cleanup
        uint64_t rflags = spin_lock(&ctx.child->cleanup_lock);
        task_scheduler::task_search_tree.remove(ctx.child->pid);
        delete ctx.child;
        set_cpu_flags(rflags);
    }
    

    return ctx.pid;
}

REGISTER_SYSCALL(SYS_wait4, sys_wait4);