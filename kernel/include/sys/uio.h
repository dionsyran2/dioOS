#pragma once
#include <stdint.h>
#include <stddef.h>

#define UIO_MAXIOV 1024        // Standard POSIX limit
#define MAX_WRITE_CHUNK 4096   // 4KB stream chunk

struct iovec {
    void *iov_base; // Pointer to the userspace buffer
    size_t iov_len; // Size of this specific buffer
};