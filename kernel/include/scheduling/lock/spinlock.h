#pragma once
#include <stdint.h>
#include <stddef.h>
#include <ArrayList.h>

typedef volatile int spinlock_t;

void spin_lock(spinlock_t* lock);
void spin_unlock(spinlock_t* lock);