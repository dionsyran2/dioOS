#include <syscalls/syscalls.h>

int tgkill(pid_t tgid, pid_t pid, int sig){
    sig--;
    if (sig < 0 || sig > 63) return -EINVAL;
    task_t *target = task_scheduler::get_thread(tgid, pid);
    if (!target || target->task_state == ZOMBIE) return -ESRCH;

    target->pending_signals |= (1UL << sig);
    return 0;
}

REGISTER_SYSCALL(SYS_tgkill, tgkill);