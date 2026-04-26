#include <scheduling/spinlock/spinlock.h>
#include <cpu.h>

uint64_t spin_lock(spinlock_t* lock) {
    // Get the current flags
    uint64_t flags = get_cpu_flags();
    
    // We must do this before trying to acquire the lock to prevent
    // an ISR on this core from trying to take the same lock.
    asm volatile ("cli"); 

    while (1) {
        int expected = 0;

        if (__atomic_compare_exchange_n(lock, &expected, 1, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            break;
        }
        
        asm volatile ("pause");
    }

    // Return the state so we know whether to re-enable them later
    return flags;
}

#include <kstdio.h>

void spin_unlock(spinlock_t *lock, uint64_t flags) {
    // Release the lock
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
    
    // Restore cpu flags
    if (flags & (1 << 8)){
        serialf("flags: 0x%x\n", flags);
    }
    set_cpu_flags(flags);
}