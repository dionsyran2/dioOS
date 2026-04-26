#include <syscalls/syscalls.h>
#include <syscalls/linux/futex.h>
#include <drivers/timers/common.h>

// Maybe a bst or a bsa would be better... you know, O(logN) but who cares, it works
struct futex_waiter{
    task_t *task;

    futex_waiter *next;
    futex_waiter *previous;
};

struct futex_t{
    spinlock_t lock;
    uint64_t key;
    futex_waiter *waiters;

    futex_t *next;
};

spinlock_t futex_lock;
futex_t *futex_list;

futex_t *get_futex(uint64_t key, bool create){
    futex_t *ret = nullptr;
    
    for (futex_t *current = futex_list; current != nullptr; current = current->next){
        if (current->key == key){
            ret = current;
            break;
        }
    }

    if (!ret && create){
        ret = new futex_t();
        ret->key = key;

        ret->next = futex_list;
        futex_list = ret;
    }

    return ret;
}

long futex_wait(uint32_t *uaddr, int futex_op, uint32_t val, const timespec *timeout){
    task_t *self = task_scheduler::get_current_task();
    uint64_t key = self->ptm->getPhysicalAddress(uaddr);

    // Get/Create the entry (Locked globally)
    uint64_t rflags_global = spin_lock(&futex_lock);
    futex_t *entry = get_futex(key, true);
    spin_unlock(&futex_lock, rflags_global);

    // Lock the specific futex entry
    uint64_t rflags = spin_lock(&entry->lock);

    // Check the value
    uint32_t current_val;
    if (!self->read_memory(uaddr, &current_val, sizeof(uint32_t))) {
        spin_unlock(&entry->lock, rflags);
        return -EFAULT;
    }

    if (current_val != val) {
        spin_unlock(&entry->lock, rflags);
        return -EAGAIN;
    }

    // Add to wait list
    futex_waiter *waiter = new futex_waiter();
    waiter->task = self;
    waiter->next = entry->waiters;
    waiter->previous = nullptr;
    if (entry->waiters) entry->waiters->previous = waiter; // Safety check
    entry->waiters = waiter;

    asm ("cli");

    self->previous_state = self->task_state;
    self->task_state = BLOCKED;
    self->block_type = WAITING_ON_FUTEX;
    self->block_context = 0;

    spin_unlock(&entry->lock, rflags);

    int ret = 0;

    // Block
    if (timeout){
        //serialf("[FUTEX] TIMED WAIT key: %p, val: %d\n", key, val);
        timespec time;
        if (!self->read_memory(timeout, &time, sizeof(timespec))){
            rflags = spin_lock(&entry->lock);

            if (waiter->previous) waiter->previous->next = waiter->next;
            else entry->waiters = waiter->next;
            if (waiter->next) waiter->next->previous = waiter->previous;

            delete waiter;
            spin_unlock(&entry->lock, rflags);
            return -EFAULT;
        }

        uint64_t start_time = GetTicks();
        uint64_t ms_timeout = (time.tv_sec * 1000) + (time.tv_nsec / 1000000);
        // Timed

        uint64_t final_ticks = local_apic_list->tick_count + (ms_timeout * local_apic_list->ticks_per_ms);
        self->schedule_until = final_ticks;

        task_scheduler::_swap_tasks();
        
        if (start_time + ms_timeout <= GetTicks())
            ret = -ETIMEDOUT;
    } else {
        // Indefinite
        //serialf("[FUTEX] WAIT key: %p, val: %d\n", key, val);
        task_scheduler::_swap_tasks();
    }

    if (self->signal_count == 0 && self->woke_by_signal){
        ret = -EINTR;
    }

    rflags = spin_lock(&entry->lock);
    // Standard doubly-linked list removal logic
    if (waiter->previous) waiter->previous->next = waiter->next;
    else entry->waiters = waiter->next;
    if (waiter->next) waiter->next->previous = waiter->previous;
    
    delete waiter;
    spin_unlock(&entry->lock, rflags);

    return ret;
}

long futex_wake(uint32_t *uaddr, int futex_op, uint32_t val){
    task_t *self = task_scheduler::get_current_task();

    // Get the physical address (the key)
    uint64_t key = self->ptm->getPhysicalAddress(uaddr);

    // Get the futex entry
    futex_t *entry = get_futex(key, false);

    //serialf("[FUTEX] WAKE key: %p, val: %d\n", key, val);

    if (!entry) {serialf("NO ENTRY\n"); return 0;}

    uint64_t rflags = spin_lock(&entry->lock);

    int woken = 0;
    for (futex_waiter *waiter = entry->waiters; waiter != nullptr; waiter = waiter->next){
        if (woken >= val) break;

        waiter->task->Unblock();
        woken++;
    }

    spin_unlock(&entry->lock, rflags);

    //serialf("WOKEN: %d\n", woken);
    return woken;
}

long futex(uint32_t *uaddr, int futex_op, uint32_t val, const timespec *timeout, uint32_t *uaddr2, uint32_t val3){
    uint32_t val2 = (uint32_t)((uint64_t)timeout);

    int operation = futex_op & FUTEX_CMD_MASK;

    switch (operation){
        case FUTEX_WAIT:
            return futex_wait(uaddr, futex_op, val, timeout);
        case FUTEX_WAKE:
            return futex_wake(uaddr, futex_op, val);
        default:
            serialf("[FUTEX] OP (%d) NOT SUPPORTED\n", operation);
            return -EOPNOTSUPP;
    }
}

REGISTER_SYSCALL(SYS_futex, futex);