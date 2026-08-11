#pragma once
#include <stdint.h>
#include <stddef.h>

#include <scheduling/spinlock/spinlock.h>
#include <structures/lists/linked_list.h>

struct task_t;

struct mutex_t {
    spinlock_t spinlock = 0;
    task_t* owner = nullptr;
    kstd::linked_list_t<task_t*> wait_queue;

    bool lock(uint64_t timeout_ms = 0);
    
    void unlock();
};