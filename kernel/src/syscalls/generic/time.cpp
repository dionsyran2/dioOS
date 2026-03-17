#include <syscalls/syscalls.h>
#include <drivers/timers/common.h>

typedef long int time_t;


// Returns/Copies to 'tloc' the current time 
long time(time_t* tloc){
    task_t* self = task_scheduler::get_current_task();

    if (tloc != nullptr) self->write_memory(tloc, &current_time, sizeof(time_t));

    return current_time;
}

REGISTER_SYSCALL(SYS_time, time);

long sys_gettimeofday(timeval *tv, timezone *tz){
    task_t *self = task_scheduler::get_current_task();

    timeval t;
    t.tv_sec = current_time;
    t.tv_usec = TSC::get_time_ns() / 1000;

    if (tv) self->write_memory(tv, &t, sizeof(t));
    return 0;
}
REGISTER_SYSCALL(SYS_gettimeofday, sys_gettimeofday);

long sys_clock_gettime(int clockid, struct timespec *tp){
    task_t *self = task_scheduler::get_current_task();

    timespec t;
    t.tv_sec = current_time;
    t.tv_nsec = TSC::get_time_ns();

    if (tp) self->write_memory(tp, &t, sizeof(timespec));

    return 0;
}
REGISTER_SYSCALL(SYS_clock_gettime, sys_clock_gettime);