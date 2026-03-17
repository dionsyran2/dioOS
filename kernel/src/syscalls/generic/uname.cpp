#include <syscalls/syscalls.h>
#include <uname.h>

long sys_uname(struct utsname *buf){
    task_t* self = task_scheduler::get_current_task();

    self->write_memory(buf, get_uname(), sizeof(utsname));
    return 0;
}

REGISTER_SYSCALL(SYS_uname, sys_uname);