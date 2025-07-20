#pragma once
#include <stdint.h>

#define IA32_FS_BASE        0xC0000100
#define IA32_GS_BASE        0xC0000101

#define IA32_EFER           0xC0000080
#define IA32_STAR           0xC0000081
#define IA32_LSTAR          0xC0000082
#define IA32_CSTAR          0xC0000083
#define IA32_SFMASK         0xC0000084

#define IA32_EFER_NXE (1ULL << 11)


static inline uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    asm volatile ("rdmsr"
                  : "=a"(low), "=d"(high)
                  : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(value >> 32);
    asm volatile ("wrmsr"
                  :
                  : "c"(msr), "a"(low), "d"(high));
}

inline bool are_interrupts_enabled() {
    unsigned long flags;
    asm volatile (
        "pushf\n\t"
        "pop %0"
        : "=r"(flags)
    );
    return flags & (1 << 9);
}
