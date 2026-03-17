#include <syscalls/syscalls.h>
#include <drivers/timers/common.h>

long clock_nanosleep(int clockid, int flags, const struct timespec *t, struct timespec *remain){
    task_t* self = task_scheduler::get_current_task();

    timespec time;
    self->read_memory((void*)t, &time, sizeof(timespec));
    uint64_t total_time = (time.tv_sec * 1000) + (time.tv_nsec / 1000000);

    self->ScheduleFor(total_time, BLOCKED);

    return 0;
}
REGISTER_SYSCALL(SYS_clocknanosleep, clock_nanosleep);