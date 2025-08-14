#include <syscalls/syscalls.h>

int sys_rt_sigprocmask(int how, const uint64_t *set, uint64_t *oldset, size_t sigsetsize) {

    task_t* ctask = task_scheduler::get_current_task();

    if (oldset) *oldset = ctask->signal_mask;

    if (set == nullptr) return 0;

    if (set) {
        switch (how) {
            case SIG_BLOCK:
                ctask->signal_mask |= *set;
                break;
            case SIG_UNBLOCK:
                ctask->signal_mask &= ~(*set);
                break;
            case SIG_SETMASK:
                ctask->signal_mask = *set;
                break;
            default:
                return -EINVAL;
        }
    }

    return 0;
}

int sys_getrlimit(int resource, struct rlimit *rlim){
    switch (resource) {
        case 7: // max open fds
            rlim->rlim_cur = INT32_MAX;
            rlim->rlim_max = INT32_MAX;
            return 0;
    }
    return -ENOSYS;
}

int sys_rt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact, size_t sigsetsize){
    if (signum <= 0 || signum >= 64) {
        return -EINVAL;
    }
    task_t* ctask = task_scheduler::get_current_task();
    
    if (act != nullptr){
        ctask->signals[signum].__sa_handler = act->__sa_handler;
        ctask->signals[signum].sa_flags = act->sa_flags;
        ctask->signals[signum].sa_mask = act->sa_mask;
        ctask->signals[signum].sa_restorer = act->sa_restorer;
    }

    return 0;
}

int sys_prlimit64(){
    return -ENOSYS;
}

int sys_kill(int pid, int sig){
    task_t* ctask = task_scheduler::get_current_task();
    task_t* proc = task_scheduler::get_process(pid);
    if (proc == nullptr) return -ESRCH;
    proc->pending_signals |= (1 << sig);
    
    if (ctask == proc){ // if its calling it for itself, swap the tasks so it executes before returning
        ctask->counter = 0;
        asm ("sti");
        asm ("int $0x23");
    }

    return 0;
}

int sys_sigreturn(){
    task_t* ctask = task_scheduler::get_current_task();

    task_t* task = task_list;
    while(task != nullptr){

        if (task->pid == ctask->pid && task->tid == ctask->tid && task != ctask){ // we found the interrupted task
            task->executing_a_handler = false;
            task->state = PAUSED;
            break;
        }
        task = task->next;
    }

    ctask->state = DISABLED;
    task_scheduler::remove_task(ctask);
    ctask->counter = 0;
    asm ("sti");
    while(1); // unreachable
}

int sys_sigaltstack(const stack_t* ss, stack_t* old_ss){ // set and/or get signal stack context
    task_t* ctask = task_scheduler::get_current_task();
    if (ctask->sig_stack && old_ss != nullptr) old_ss->ss_sp = (void*)(ctask->sig_stack + TASK_SCHEDULER_DEFAULT_STACK_SIZE);
    if (ss == nullptr) return 0;
    ctask->sig_sp = (uint64_t)ss->ss_sp;
    return 0;
}


void register_sig_syscalls(){
    register_syscall(SYSCALL_SIGPROCMASK, (syscall_handler_t)sys_rt_sigprocmask);
    register_syscall(SYSCALL_GETRLIMIT, (syscall_handler_t)sys_getrlimit);
    register_syscall(SYSCALL_SIGACTION, (syscall_handler_t)sys_rt_sigaction);
    register_syscall(SYSCALL_PRLIMIT, (syscall_handler_t)sys_prlimit64);
    register_syscall(SYSCALL_KILL, (syscall_handler_t)sys_kill);
    register_syscall(SYSCALL_SIGRETURN, (syscall_handler_t)sys_sigreturn);
    register_syscall(SYSCALL_SIGALTSTACK, (syscall_handler_t)sys_sigaltstack);
}