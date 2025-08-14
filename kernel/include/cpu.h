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

inline void enable_nx() {
    uint64_t efer = read_msr(IA32_EFER);
    efer |= IA32_EFER_NXE;
    write_msr(IA32_EFER, efer);
}

inline void enable_simd() {
    uint64_t cr0, cr4;

    // Enable FPU, clear EM, set MP in CR0
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // EM = 0 (disable emulation)
    cr0 |=  (1 << 1); // MP = 1 (monitor coprocessor)
    asm volatile ("mov %0, %%cr0" :: "r"(cr0));

    // Enable SSE and AVX in CR4
    asm volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // OSFXSR (enables SSE instructions)
    cr4 |= (1 << 10); // OSXMMEXCPT (enables SSE exceptions)
    asm volatile ("mov %0, %%cr4" :: "r"(cr4));

    // Initialize FPU
    asm volatile ("fninit");

    // Enable AVX (XCR0[2]), SSE (XCR0[1]), x87 (XCR0[0]) if supported
    uint32_t eax, ebx, ecx, edx;

    // CPUID.(EAX=0x1):ECX[bit 27] = OSXSAVE
    asm volatile (
        "mov $1, %%eax\n"
        "cpuid\n"
        : "=c"(ecx)
        : 
        : "eax", "ebx", "edx"
    );

    if (ecx & (1 << 27)) { // OSXSAVE supported
        // Check XCR0 to see what we're allowed to set
        uint64_t xcr0;
        asm volatile (
            "xor %%ecx, %%ecx\n"
            "xgetbv\n"
            : "=a"(eax), "=d"(edx)
            :
            : "ecx"
        );
        xcr0 = ((uint64_t)edx << 32) | eax;

        // Enable x87 (bit 0), SSE (bit 1), AVX (bit 2)
        xcr0 |= 0b111;

        eax = (uint32_t)(xcr0 & 0xFFFFFFFF);
        edx = (uint32_t)(xcr0 >> 32);
        asm volatile (
            "xor %%ecx, %%ecx\n"
            "xsetbv\n"
            :
            : "a"(eax), "d"(edx)
            : "ecx"
        );
    }
}