#pragma once
#include <stdint.h>
#include <stddef.h>

struct task_t;

enum lock_type_t {
    LOCK_READ, // Shared
    LOCK_WRITE, // Exclusive
};

struct file_lock_t {
    uint64_t start;
    uint64_t end;
    lock_type_t type;
    task_t *owner;
};
