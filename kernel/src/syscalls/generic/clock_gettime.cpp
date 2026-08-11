#include <syscalls/syscalls.h>
#include <drivers/timers/common.h>
#include <time.h>

#define CLOCK_REALTIME      0
#define CLOCK_MONOTONIC     1

int clock_gettime(int clockid, struct timespec *tp){
    task_t *self = task_scheduler::get_current_task();

    timespec t;
    
    if (clockid == CLOCK_REALTIME) { 
        t.tv_sec = current_time;
        t.tv_nsec = TSC::get_time_ns();
    } else if (clockid == CLOCK_MONOTONIC) {
        t.tv_sec = time_since_boot;
        t.tv_nsec = TSC::get_time_ns();
    }

    if (tp) self->write_to_userspace(tp, &t, sizeof(timespec));

    return 0;
}

REGISTER_SYSCALL(SYS_clock_gettime, clock_gettime);