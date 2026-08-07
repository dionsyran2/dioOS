#pragma once
#include <stdint.h>
#include <stddef.h>

#include <scheduling/spinlock/spinlock.h>
#include <structures/lists/linked_list.h>

struct task_t;

struct semaphore_t {
    spinlock_t lock = 0;
    int count = 0;
    kstd::linked_list_t<task_t*> wait_queue;

    semaphore_t(int initial_count = 0) : lock(0), count(initial_count) {}

    // Releases a resource token, waking up the next thread in FIFO order
    void post();

    // Acquires a resource token. Returns true on success, false on timeout.
    // Specifying a timeout of -1 (UINT64_MAX) results in an infinite wait.
    bool wait(uint64_t timeout_ms = -1);
};