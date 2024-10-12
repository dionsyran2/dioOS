#include "kernelUtil.h"
#include "memory/heap.h"
#include "scheduling/pit/pit.h"
#include "renderer/AdvancedRenderer.h"
#include "cpu.h"

uint32_t seed = 51451;

const uint64_t a = 1664525; // Multiplier
const uint64_t c = 1013904223; // Increment
const uint64_t m = 4294967296; // 2^32

uint64_t next() {
    seed = (a * seed + c) % m;
    return seed;
}

    // Function to get a random number within a specific range [min, max]
uint64_t getRandomInRange(uint32_t min, uint32_t max) {
    if (min > max) {
        return 0; // Return 0 or handle error if the range is invalid
    }
    uint32_t range = max - min + 1; // Calculate the range size
    return (next() % range) + min; // Return random number in the specified range
}


extern "C" void _start(BootInfo* bootInfo){
    KernelInfo kernelInfo = InitializeKernel(bootInfo);
    PageTableManager* pageTableManager = kernelInfo.pageTableManager;
    CPU cpu = get_cpu_info();
    globalRenderer->print("CPU INFO: \n");
    globalRenderer->print("Vendor: ");
    globalRenderer->print((const char*)cpu.vendor);
    globalRenderer->print("\n");
    globalRenderer->print("CPU Type: ");
    globalRenderer->print(toString((uint64_t)cpu.type));
    globalRenderer->print("\n");
    globalRenderer->print("CPU model: ");
    globalRenderer->print(toString((uint64_t)cpu.model));
    globalRenderer->print("\n");
    globalRenderer->print("CPU family: ");
    globalRenderer->print(toString((uint64_t)cpu.family));
    globalRenderer->print("\n");
    globalRenderer->print("CPU stepping: ");
    globalRenderer->print(toString((uint64_t)cpu.stepping));
    globalRenderer->print("\n");
    globalRenderer->print("CPU Cores: ");
    globalRenderer->print(toString((uint64_t)cpu.coresPerPackage));
    globalRenderer->print("\n");
    globalRenderer->print("Logical Processors (Threads): ");
    globalRenderer->print(toString((uint64_t)cpu.logicalProcessors));
    globalRenderer->print("\n");
    globalRenderer->print("Number of packages: ");
    globalRenderer->print(toString((uint64_t)cpu.numOfPackages));
    globalRenderer->print("\n");
    globalRenderer->print("Hyper Thread: ");
    globalRenderer->print(cpu.HT == true?"Enabled":"Disabled / Unsupported");
    globalRenderer->print("\n");
    globalRenderer->print("featureFlags: ");
    globalRenderer->print(toHString(cpu.featureFlags));
    globalRenderer->print("\n");
    globalRenderer->print("extendedModel: ");
    globalRenderer->print(toHString(cpu.extendedModel));
    globalRenderer->print("\n");
    globalRenderer->print("extendedFamily: ");
    globalRenderer->print(toHString(cpu.extendedFamily));
    globalRenderer->print("\n");
    globalRenderer->print("Sending TEST INTERRUPT TO CPU!\n");
    SendIPI(0x30, 0);
    SendIPI(0x30, 0);
    SendIPI(0x32, 2);
    InitAllCPUs();
    globalRenderer->print("TEST INTERRUPT FINISHED!\n");


    //PIT::Sleep(10000);
    //globalRenderer->Clear(0);
    //PIT::Sleep(5000);
    //AdvancedRenderer advRend = AdvancedRenderer(bootInfo->framebuffer, bootInfo->psf1_Font);
    while(1){

    };

}