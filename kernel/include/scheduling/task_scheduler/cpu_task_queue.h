#pragma once
#include <stdint.h>
#include <stddef.h>
#include <memory/heap.h>
#include <memory.h>

#include <scheduling/task_scheduler/task_scheduler.h>

#define AMOUNT_TO_STEAL(x) (x / 2)
#define AMOUNT_TO_EXPAND(x) (x * 2)

struct task_t;

class cpu_task_queue_t {
    public:

    cpu_task_queue_t(size_t size);
    void push(task_t *task);
    task_t *pop();
    task_t *steal(cpu_task_queue_t *victim);
    void _expand_ring(bool lock_held = false);
    uint64_t _lock();
    void _unlock(uint64_t flags);

    private:
    spinlock_t lock = 0;
    
    public:
    size_t write_ptr = 0;
    size_t read_ptr = 0;
    size_t entry_count = 0;
    
    task_t **queue;
    size_t queue_size;
};