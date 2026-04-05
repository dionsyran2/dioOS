#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cpuid.h>

#define IA32_FS_BASE        0xC0000100
#define IA32_GS_BASE        0xC0000101
#define IA32_GS_KERNEL_BASE 0xC0000102


#define IA32_EFER           0xC0000080
#define IA32_STAR           0xC0000081
#define IA32_LSTAR          0xC0000082
#define IA32_CSTAR          0xC0000083
#define IA32_SFMASK         0xC0000084

#define IA32_EFER_NXE (1ULL << 11)

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800


extern bool is_sse_enabled;
extern bool is_avx_enabled;
extern bool is_avx2_enabled;
extern bool is_avx512_enabled;


extern "C" bool _enable_sse();
extern "C" bool _enable_avx();
extern "C" bool _enable_avx512();
extern "C" bool _cpu_has_avx2();

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

inline uint64_t get_cpu_flags(){
    unsigned long flags;
    asm volatile (
        "pushf\n\t"
        "pop %0"
        : "=r"(flags)
    );
    
    return flags;
}

inline void set_cpu_flags(uint64_t flags){
    asm volatile (
        "pushq %0\n\t"
        "popfq"
        :
        : "r"(flags)
        : "cc", "memory"
    );
}

inline void enable_nx() {
    uint64_t efer = read_msr(IA32_EFER);
    efer |= IA32_EFER_NXE;
    write_msr(IA32_EFER, efer);
}

static inline void cpuid(int leaf, int subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __cpuid_count(leaf, subleaf, *eax, *ebx, *ecx, *edx);
}


/* A function to invalidate a specific tlb */
static inline void __native_flush_tlb_single(unsigned long addr) {
   asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}

extern size_t g_fpu_storage_size;
inline void save_fpu_state(void* buffer) {
    uint32_t low, high;
    
    asm volatile (
        "xgetbv" 
        : "=a"(low), "=d"(high) 
        : "c"(0) // XCR0
    );

    // It checks internal hardware flags. If registers weren't modified,
    // it skips the write to memory. FAST.
    asm volatile (
        "xsaveopt64 (%0)" 
        : 
        : "r"(buffer), "a"(low), "d"(high) 
        : "memory"
    );
}

inline void restore_fpu_state(void* buffer) {
    // 1. Get the allowed features from XCR0
    // (We read XCR0 index 0, which contains the user-state bitmap)
    uint32_t low, high;
    
    asm volatile (
        "xgetbv" 
        : "=a"(low), "=d"(high) 
        : "c"(0) // XCR0
    );

    // 2. Restore using the valid mask
    asm volatile (
        "xrstor64 (%0)" 
        : 
        : "r"(buffer), "a"(low), "d"(high) 
        : "memory"
    );
}



inline void detect_fpu_area_size() {
    uint32_t eax, ebx, ecx, edx;
    
    // CPUID Leaf 0xD, Subleaf 0
    // EBX = Max size required by currently enabled features (in XCR0)
    // ECX = Max size required by ALL supported features
    asm volatile("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0x0D), "c"(0)
    );

    // Use EBX if you have already set up XCR0, or ECX to be safe/future-proof.
    // We strictly align up to 64 bytes just to be safe.
    g_fpu_storage_size = ecx; 
}

typedef void (*function)(void);
typedef uint64_t __register;

struct __registers_t{
    uint64_t cr3;
    
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t rip;
    uint64_t CS;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t SS;
} __attribute__((packed));