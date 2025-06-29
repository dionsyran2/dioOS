#pragma once
#include <stdint.h>
#include <stddef.h>

#define ARCH_SET_FS     0x1002
#define ARCH_GET_FS     0x1003
#define ARCH_SET_GS     0x1001
#define ARCH_GET_GS     0x1004


#define SYSCALL_READ                     0
#define SYSCALL_LSEEK                    8
#define SYSCALL_MMAP                     9
#define SYSCALL_ARCH_PRCTL               158
#define SYSCALL_SET_TID_ADDR             218
#define SYSCALL_BRK                      12
#define SYSCALL_MPROTECT                 10
#define SYSCALL_WRITEV                   20
#define SYSCALL_IOCTL                    16
#define SYSCALL_EXIT_GROUP               231
#define SYSCALL_GETTIME                  228

#define MAP_SHARED       0x01  // Share changes
#define MAP_PRIVATE      0x02  // Changes are private (copy-on-write)
#define MAP_FIXED        0x10  // Force use of addr
#define MAP_ANONYMOUS    0x20  // Not backed by file (fd must be -1)

#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4

#define TIOCGWINSZ 0x5413

struct iovec {
    void *iov_base;
    size_t iov_len;
};

struct winsize {
    unsigned short ws_row;    // number of rows (lines) in the terminal
    unsigned short ws_col;    // number of columns in the terminal
    unsigned short ws_xpixel; // width in pixels (may be zero or not supported)
    unsigned short ws_ypixel; // height in pixels (may be zero or not supported)
};

struct timespc{
    uint64_t tv_sec;
    uint32_t tv_nsec;
};

void setup_syscalls();
extern "C" void syscall_entry();

extern "C" uint64_t next_syscall_stack;
extern "C" uint64_t user_syscall_stack;