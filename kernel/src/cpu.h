#pragma once
#include <stdint.h>
class CPU{
    public:
    char vendor[13];
    uint32_t stepping;
    uint32_t model;
    uint32_t family;
    uint32_t type;
    uint32_t logicalProcessors;
    uint32_t coresPerPackage;
    uint32_t cacheLineSize;
    uint16_t numOfPackages;
    uint32_t featureFlags;
    uint32_t extendedModel;
    uint32_t extendedFamily;
    bool HT;
};
void cpuSetMSR(uint32_t msr, uint32_t value, uint32_t high);
void cpuGetMSR(uint32_t msr, uint32_t* value, uint32_t* high);
CPU get_cpu_info();

void cpuid(uint32_t code, uint32_t subleaf, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d);
void cpuid(uint32_t code, uint32_t subleaf, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d);