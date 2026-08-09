#pragma once
#include <stdint.h>
#include <elf/bits.h>

inline uint32_t get_hwcap_x86() {
    uint32_t eax, ebx, ecx, edx;
    uint32_t hwcap = 0;

    // CPUID leaf 0x00000001: Feature Information
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(1), "c"(0));

    if (edx & (1 << 0))  hwcap |= HWCAP_X86_FPU;
    if (edx & (1 << 1))  hwcap |= HWCAP_X86_VME;
    if (edx & (1 << 2))  hwcap |= HWCAP_X86_DE;
    if (edx & (1 << 3))  hwcap |= HWCAP_X86_PSE;
    if (edx & (1 << 4))  hwcap |= HWCAP_X86_TSC;
    if (edx & (1 << 5))  hwcap |= HWCAP_X86_MSR;
    if (edx & (1 << 6))  hwcap |= HWCAP_X86_PAE;
    if (edx & (1 << 7))  hwcap |= HWCAP_X86_MCE;
    if (edx & (1 << 8))  hwcap |= HWCAP_X86_CX8;
    if (edx & (1 << 9))  hwcap |= HWCAP_X86_APIC;
    if (edx & (1 << 11)) hwcap |= HWCAP_X86_SEP;
    if (edx & (1 << 12)) hwcap |= HWCAP_X86_MTRR;
    if (edx & (1 << 13)) hwcap |= HWCAP_X86_PGE;
    if (edx & (1 << 14)) hwcap |= HWCAP_X86_MCA;
    if (edx & (1 << 15)) hwcap |= HWCAP_X86_CMOV;
    if (edx & (1 << 16)) hwcap |= HWCAP_X86_PAT;
    if (edx & (1 << 18)) hwcap |= HWCAP_X86_PSN;
    if (edx & (1 << 19)) hwcap |= HWCAP_X86_CLFSH;
    if (edx & (1 << 21)) hwcap |= HWCAP_X86_DS;
    if (edx & (1 << 22)) hwcap |= HWCAP_X86_ACPI;
    if (edx & (1 << 23)) hwcap |= HWCAP_X86_MMX;
    if (edx & (1 << 24)) hwcap |= HWCAP_X86_FXSR;
    if (edx & (1 << 25)) hwcap |= HWCAP_X86_SSE;
    if (edx & (1 << 26)) hwcap |= HWCAP_X86_SSE2;
    if (edx & (1 << 27)) hwcap |= HWCAP_X86_SS;
    if (edx & (1 << 28)) hwcap |= HWCAP_X86_HTT;
    if (edx & (1 << 29)) hwcap |= HWCAP_X86_TM;
    if (edx & (1 << 31)) hwcap |= HWCAP_X86_PBE;

    return hwcap;
}