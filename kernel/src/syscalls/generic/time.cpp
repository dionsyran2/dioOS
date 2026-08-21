#include <syscalls/syscalls.h>
#include <drivers/timers/common.h>

time_t time(time_t *tloc){
    task_t *self = task_scheduler::get_current_task();

    time_t t = current_time;
    if (tloc){
        self->write_to_userspace(tloc, &t, sizeof(time_t));
    }

    return t;
}

REGISTER_SYSCALL(SYS_time, time);