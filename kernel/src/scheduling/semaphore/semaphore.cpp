#include <scheduling/semaphore/semaphore.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <drivers/timers/common.h>
#include <scheduling/apic/lapic.h>
#include <kerrno.h>

void semaphore_t::post() {
    uint64_t flags = spin_lock(&this->lock);

    // If there are threads waiting, perform a direct token handoff
    if (this->wait_queue.size() > 0) {
        task_t* next_task = this->wait_queue.get(0);
        this->wait_queue.remove(0);

        // DIRECT HANDOFF: We don't increment this->count because the token 
        // goes straight to the waking task. This prevents priority inversion and barging.
        spin_unlock(&this->lock, flags);

        next_task->unblock();
        return;
    }

    // No one is waiting; increment the raw token count
    this->count++;
    spin_unlock(&this->lock, flags);
}

bool semaphore_t::wait(uint64_t timeout_ms) {
    task_t* self = task_scheduler::get_current_task();

    // Early boot / Interrupt context / No-task fallback
    if (self == nullptr) {
        uint64_t ticks_to_wait = timeout_ms * local_apic_list->ticks_per_ms;
        uint64_t end_ticks = local_apic_list->tick_count + ticks_to_wait;

        while (true) {
            uint64_t flags = spin_lock(&this->lock);
            if (this->count > 0) {
                this->count--;
                spin_unlock(&this->lock, flags);
                return true;
            }
            spin_unlock(&this->lock, flags);

            // Break if we hit a valid timeout limit
            if (timeout_ms != (uint64_t)-1 && local_apic_list->tick_count >= end_ticks) {
                break;
            }
            
            // Emit a PAUSE instruction to optimize pipeline resource allocation on physical cores
            asm volatile("pause");
        }
        return false;
    }

    // Standard task scheduler execution path
    uint64_t flags = spin_lock(&this->lock);

    // Fast Path: A token is immediately available
    if (this->count > 0) {
        this->count--;
        spin_unlock(&this->lock, flags);
        return true;
    }

    // Slow Path: No tokens available. We must queue and sleep
    this->wait_queue.add(self);

    // Relies on your unified task blocking/unblocking scheduler interface
    if (timeout_ms != (uint64_t)-1 && timeout_ms > 0) {
        self->block(timeout_ms, nullptr);
    } else {
        self->block();
    }

    // Re-acquire lock to safely inspect post-block state
    flags = spin_lock(&this->lock);

    // Inspect if we woke up because of a timeout event
    if (self->block_status == -ETIMEDOUT) {
        for (int i = 0; i < this->wait_queue.size(); i++) {
            if (this->wait_queue.get(i) == self) {
                this->wait_queue.remove(i);
                break;
            }
        }
        spin_unlock(&this->lock, flags);
        return false;
    }

    // Woke up normally via post(); the token was already decremented/allocated to us!
    spin_unlock(&this->lock, flags);
    return true;
}