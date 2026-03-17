#include <scheduling/semaphore/semaphore.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <drivers/timers/common.h>
#include <scheduling/apic/lapic.h>

void semaphore_t::post(){
    uint64_t rflags = spin_lock(&this->lock);

    this->count++;

    spin_unlock(&this->lock, rflags);
}

bool semaphore_t::wait(uint64_t timeout){
    task_t* self = task_scheduler::get_current_task();
    uint64_t end_ticks = local_apic_list->tick_count + (timeout * local_apic_list->ticks_per_ms);

    if (self == nullptr){
        while(1){
            Sleep(1);

            uint64_t rflags = spin_lock(&this->lock);
            if (this->count > 0){
                this->count--;
                spin_unlock(&this->lock, rflags);
                return true;
            }
            
            spin_unlock(&this->lock, rflags);

            if (local_apic_list->tick_count >= end_ticks) break;
        }
    }else{
        
        while(1){
            self->ScheduleFor(timeout, BLOCKED, MEMORY_NON_ZERO, (uint64_t)&this->count);

            uint64_t rflags = spin_lock(&this->lock);
            if (this->count > 0){
                this->count--;
                spin_unlock(&this->lock, rflags);
                return true;
            }

            spin_unlock(&this->lock, rflags);

            if (local_apic_list->tick_count >= end_ticks) break;
        }
    }

    return false;
}