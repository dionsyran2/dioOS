#include <scheduling/spinlock/spinlock.h>
#include <cpu.h>

uint64_t spin_lock(spinlock_t* lock) {
    uint64_t flags = get_cpu_flags();
    asm volatile ("cli"); 

    int expected = 0;

    while (1) {
        if (__atomic_load_n(lock, __ATOMIC_RELAXED) == 0) {
            
            expected = 0; 
            
            if (__atomic_compare_exchange_n(lock, &expected, 1, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                break;
            }
        }
        
        asm volatile ("pause");
    }

    return flags;
}

void spin_unlock(spinlock_t *lock, uint64_t flags) {
    // Release the lock
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
    
    // Restore cpu flags
    set_cpu_flags(flags);
}


void spinlock2_t::lock(){
    this->rflags = spin_lock(&this->slock);
}

void spinlock2_t::unlock(){
    spin_unlock(&this->slock, this->rflags);
}