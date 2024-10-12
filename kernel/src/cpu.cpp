#include "cpu.h"
#include "BasicRenderer.h"
// Assembly function to call CPUID
void cpuid(uint32_t code, uint32_t subleaf, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    __asm__ volatile("cpuid"
                     : "=a" (*a), "=b" (*b), "=c" (*c), "=d" (*d)
                     : "a" (code), "c" (subleaf));
}

void debug_cpuid_values() {
    uint32_t eax, ebx, ecx, edx;

    // CPUID 0xB, Sub-leaf 0 (SMT Level)
    cpuid(0xB, 0, &eax, &ebx, &ecx, &edx);
    globalRenderer->print("CPUID 0xB Sub-leaf 0 (SMT Level):\n");

    globalRenderer->print("EAX: ");
    globalRenderer->print(toHString(eax)); // Print in hex
    globalRenderer->print("\n");

    globalRenderer->print("EBX: ");
    globalRenderer->print(toString((uint64_t)ebx)); // Print in decimal
    globalRenderer->print("\n");

    globalRenderer->print("ECX: ");
    globalRenderer->print(toHString(ecx)); // Print in hex
    globalRenderer->print("\n");

    globalRenderer->print("EDX: ");
    globalRenderer->print(toHString(edx)); // Print in hex
    globalRenderer->print("\n\n");

    // CPUID 0xB, Sub-leaf 1 (Core Level)
    cpuid(0xB, 1, &eax, &ebx, &ecx, &edx);
    globalRenderer->print("CPUID 0xB Sub-leaf 1 (Core Level):\n");

    globalRenderer->print("EAX: ");
    globalRenderer->print(toHString(eax)); // Print in hex
    globalRenderer->print("\n");

    globalRenderer->print("EBX: ");
    globalRenderer->print(toString((uint64_t)ebx)); // Print in decimal
    globalRenderer->print("\n");

    globalRenderer->print("ECX: ");
    globalRenderer->print(toHString(ecx)); // Print in hex
    globalRenderer->print("\n");

    globalRenderer->print("EDX: ");
    globalRenderer->print(toHString(edx)); // Print in hex
    globalRenderer->print("\n\n");

    // CPUID 0x4 (Determines cores per package for older CPUs)
    cpuid(0x4, 0, &eax, &ebx, &ecx, &edx);
    globalRenderer->print("CPUID 0x4 (Core and cache info):\n");

    globalRenderer->print("EAX: ");
    globalRenderer->print(toHString(eax)); // Print in hex
    globalRenderer->print("\n");

    globalRenderer->print("EBX: ");
    globalRenderer->print(toString((uint64_t)ebx)); // Print in decimal
    globalRenderer->print("\n");

    globalRenderer->print("ECX: ");
    globalRenderer->print(toHString(ecx)); // Print in hex
    globalRenderer->print("\n");

    globalRenderer->print("EDX: ");
    globalRenderer->print(toHString(edx)); // Print in hex
    globalRenderer->print("\n\n");
}

CPU get_cpu_info() {
    CPU cpu;
    uint32_t eax, ebx, ecx, edx;

    // Initialize vendor string to null
    cpu.vendor[0] = '\0';

    // CPUID 0: Vendor ID and max CPUID level
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    ((uint32_t*)cpu.vendor)[0] = ebx;
    ((uint32_t*)cpu.vendor)[1] = edx;
    ((uint32_t*)cpu.vendor)[2] = ecx;
    cpu.vendor[12] = '\0';

    // CPUID 1: Get basic CPU details (stepping, model, family, type)
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);

    cpu.stepping = eax & 0xF;
    cpu.model = (eax >> 4) & 0xF;
    cpu.family = (eax >> 8) & 0xF;
    cpu.type = (eax >> 12) & 0x3;
    cpu.extendedModel = (eax >> 16) & 0xF;
    cpu.extendedFamily = (eax >> 20) & 0xFF;

    // Adjust family and model
    if (cpu.family == 0xF) {
        cpu.family += cpu.extendedFamily;
    }
    if (cpu.family == 0x6 || cpu.family == 0xF) {
        cpu.model += (cpu.extendedModel << 4);
    }

    // Feature flags (such as Hyper-Threading support)
    cpu.featureFlags = edx;

    cpu.HT = (edx & (1 << 28)) != 0;

    uint32_t smtMaskWidth = 0, coreMaskWidth = 0;

    cpuid(0xB, 0, &eax, &ebx, &ecx, &edx);
    smtMaskWidth = eax & 0x1F;
    cpu.logicalProcessors = ebx & 0xFFFF;

    cpuid(0xB, 1, &eax, &ebx, &ecx, &edx);
    coreMaskWidth = eax & 0x1F;

    uint32_t totalLogicalProcessors = ebx & 0xFFFF;
    uint32_t totalCores = totalLogicalProcessors >> smtMaskWidth;

    if (cpu.HT) {
        cpu.coresPerPackage = totalCores;
        cpu.logicalProcessors = totalCores * 2;
    } else {
        cpu.coresPerPackage = totalCores;
        cpu.logicalProcessors = totalCores;
    }

    // Cache line size (CPUID 1: EBX bits 15:8)
    cpu.cacheLineSize = (ebx >> 8) & 0xFF;

    return cpu;
}

void cpuSetMSR(uint32_t msr, uint32_t value, uint32_t high) {
    asm volatile (
        "wrmsr"
        : // No outputs
        : "A"(value), "c"(msr), "d"(high) // Inputs: value in EAX, msr in ECX, high in EDX
    );
}

void cpuGetMSR(uint32_t msr, uint32_t* value, uint32_t* high) {
    asm volatile (
        "rdmsr"                 // Read from MSR
        : "=A"(*value)         // Output: EAX to *value and EDX to *high
        : "c"(msr)             // Input: MSR index in ECX
    );
}
