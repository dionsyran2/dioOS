#include <syscalls/syscalls.h>
#include <drivers/timers/common.h>
#include <signum.h>

struct itimerval {
    struct timeval it_interval; /* The reload value (The Metronome) */
    struct timeval it_value;    /* The time until the VERY FIRST ring */
};
#define ITIMER_REAL 0

int alarm(unsigned int seconds){
    task_t *self = task_scheduler::get_current_task();

    self->pending_signals &= ~(1UL << (SIGALRM - 1));
    self->kernel_signals &= ~(1UL << (SIGALRM - 1));
    self->alarm = seconds;
    self->prev_alarm = current_time;

    return 0;
}

REGISTER_SYSCALL(SYS_alarm, alarm);

int sys_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value) {
    task_t *self = task_scheduler::get_current_task();

    if (which != ITIMER_REAL) return -EINVAL; // We only support the real-time clock right now


    if (new_value) {
        struct itimerval kernel_timer;
        if (!self->read_memory((void*)new_value, &kernel_timer, sizeof(itimerval))) {
            return -EFAULT;
        }

        uint64_t initial_ms = (kernel_timer.it_value.tv_sec * 1000) + (kernel_timer.it_value.tv_usec / 1000);
        uint64_t interval_ms = (kernel_timer.it_interval.tv_sec * 1000) + (kernel_timer.it_interval.tv_usec / 1000);

        if (initial_ms == 0) {
            self->itimer_next_expiration = 0;
            self->itimer_interval = 0;
        } else {
            self->itimer_next_expiration = (TSC::get_uptime_ns() / 1000000ULL) + initial_ms;
            self->itimer_interval = interval_ms;
        }
    }

    return 0;
}
REGISTER_SYSCALL(SYS_setitimer, sys_setitimer);