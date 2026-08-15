#include <syscalls/syscalls.h>

int kill(int pid, int signum) {
    // If pid is positive, then signal sig is sent to the process with the ID specified by pid.
    if (pid > 0){
        task_t *victim = task_scheduler::search_by_pid(pid);
        if (!victim) return -ESRCH;
        victim->pending_signals |= (1UL << signum);
    }

    /*
    If pid equals 0, then sig is sent to every process in the process group of the calling process.

    If pid equals -1, then sig is sent to every process for which the calling process has permission to send signals, except for process 1 (init), but see below.

    If pid is less than -1, then sig is sent to every process in the process group whose ID is -pid.

    If sig is 0, then no signal is sent, but existence and permission checks are still performed; this can be used to check for the existence of a process ID or process group ID that the caller is permitted to signal.
    */

    return 0;
}

REGISTER_SYSCALL(SYS_kill, kill);