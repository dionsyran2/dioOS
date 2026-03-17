#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/spinlock/spinlock.h>

struct semaphore_t{
    spinlock_t lock;
    int count;

    void post();
    bool wait(uint64_t timeout = -1);
};