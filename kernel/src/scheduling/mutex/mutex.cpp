#include <scheduling/mutex/mutex.h>
#include <scheduling/apic/lapic.h>
#include <drivers/timers/common.h>

bool mutex_t::lock(uint64_t timeout){
    task_t* self = task_scheduler::get_current_task();
    uint64_t end_ticks = local_apic_list->tick_count + (timeout * local_apic_list->ticks_per_ms);

    if (self == nullptr) return false; // wtf am i supposed to do? 
    
    while(1){        
        uint64_t rflags = spin_lock(&this->spinlock);
        if (self->pid == this->owner_pid || this->owner_pid == -1){
            self->counter++;
            spin_unlock(&this->spinlock, rflags);
            return true;
        }

        spin_unlock(&this->spinlock, rflags);
        
        if (end_ticks <= local_apic_list->tick_count) break;
        Sleep(1);
    }

    return false;
}

bool mutex_t::unlock(){
    task_t* self = task_scheduler::get_current_task();
    if (self == nullptr) return false;

    if (self->pid == this->owner_pid || this->owner_pid == -1){
        uint64_t rflags = spin_lock(&this->spinlock);
        if (self->counter > 0) self->counter--;
        if (self->counter == 0) this->owner_pid = -1;
        spin_unlock(&this->spinlock, rflags);

        return true;
    }

    return false;
}