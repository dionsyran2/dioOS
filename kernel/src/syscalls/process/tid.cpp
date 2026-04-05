#include <syscalls/syscalls.h>

uint64_t sys_getpid(){
    task_t* task = task_scheduler::get_current_task();
    return task->tgid;
}

REGISTER_SYSCALL(SYS_getpid, sys_getpid);

uint64_t sys_getppid(){
    task_t* task = task_scheduler::get_current_task();
    return task->ppid < 0 ? task->pid : task->ppid;
}
REGISTER_SYSCALL(SYS_getppid, sys_getppid);


long sys_getpgid(int pid){
    task_t* task = task_scheduler::get_process(pid);
    if (pid <= 0) task = task_scheduler::get_current_task();



    if (task == nullptr) return -ESRCH;
    return task->pgid;
}
REGISTER_SYSCALL(SYS_getpgid, sys_getpgid);

long sys_setpgid(int pid, int pgid){
    task_t* task = task_scheduler::get_process(pid);
    if (pid <= 0) task = task_scheduler::get_current_task();


    if (task == nullptr) return -ESRCH;
    
    task->pgid = pgid;
    return task->pgid;
}
REGISTER_SYSCALL(SYS_setpgid, sys_setpgid);


long sys_gettid(){
    task_t* task = task_scheduler::get_current_task();
    return task->pid;
}
REGISTER_SYSCALL(SYS_gettid, sys_gettid);


long sys_getpgrp(){
    task_t* ctask = task_scheduler::get_current_task();
    return ctask->pgid;
}
REGISTER_SYSCALL(SYS_getpgrp, sys_getpgrp);

