#include <scheduling/mutex/mutex.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <kerrno.h>

bool mutex_t::lock(uint64_t timeout_ms) {
    task_t* self = task_scheduler::get_current_task();
    if (!self) return false;

    uint64_t flags = spin_lock(&this->spinlock);

    // Fast Path: Mutex is free!
    if (this->owner == nullptr) {
        this->owner = self;
        spin_unlock(&this->spinlock, flags);
        return true;
    }

    // Slow Path: Mutex is held. We must block.
    this->wait_queue.add(self);

    
    if (timeout_ms > 0) {
        self->block(timeout_ms, nullptr);
    } else {
        self->block();
    }

    // Acquire the lock again to inspect state safely
    flags = spin_lock(&this->spinlock);

    // Check if we woke up because of a timeout
    if (self->block_status == -ETIMEDOUT) {
        for (int i = 0; i < this->wait_queue.size(); i++) {
            if (this->wait_queue.get(i) == self) {
                this->wait_queue.remove(i);
                break;
            }
        }
        spin_unlock(&this->spinlock, flags);
        return false; // Lock acquisition failed due to timeout
    }

    spin_unlock(&this->spinlock, flags);
    return true; // We successfully inherited the lock
}

void mutex_t::unlock() {
    uint64_t flags = spin_lock(&this->spinlock);

    task_t* self = task_scheduler::get_current_task();
    if (this->owner != self) {
        // Bug! Trying to unlock a mutex we don't own.
        spin_unlock(&this->spinlock, flags);
        return;
    }

    // If no one is waiting, simply free the lock and exit
    if (this->wait_queue.size() == 0) {
        this->owner = nullptr;
        spin_unlock(&this->spinlock, flags);
        return;
    }

    // Pop the next thread from our wait list
    task_t* next_owner = this->wait_queue.get(0);
    this->wait_queue.remove(0);

    this->owner = next_owner;

    spin_unlock(&this->spinlock, flags);

    next_owner->unblock();
}