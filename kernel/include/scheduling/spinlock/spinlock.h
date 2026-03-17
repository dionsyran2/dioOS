#pragma once
#include <stdint.h>
#include <stddef.h>

typedef volatile int spinlock_t;

uint64_t spin_lock(spinlock_t* lock);
void spin_unlock(spinlock_t *lock, uint64_t flags);