#pragma once
#include <stdint.h>
#include <stddef.h>

#include <scheduling/spinlock/spinlock.h>
#include <scheduling/task_scheduler/task_scheduler.h>

struct mutex_t{
    spinlock_t spinlock;
    pid_t owner_pid;

    int count;
    bool lock(uint64_t timeout);
    bool unlock();
};