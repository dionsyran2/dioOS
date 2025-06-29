#include <scheduling/lock/spinlock.h>

void spin_lock(spinlock_t* lock) {
    return;
    while (1) {
        int expected = 0;

        // Try to atomically swap 0 -> 1

        if (__atomic_compare_exchange_n(lock, &expected, 1, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            break; // Lock aquired
        }
        
        __asm__ __volatile__("pause");
    }
}

void spin_unlock(spinlock_t *lock) {
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
}