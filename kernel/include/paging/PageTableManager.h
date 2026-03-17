#pragma once
#include <stdint.h>
#include <stddef.h>

#include <paging/PageEntry.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <scheduling/task_scheduler/vm_tracker.h>


/* Forward declarations */
struct task_t;
class vm_tracker_t;

class PageTableManager{
    public:
    PageTableManager(PageTable* PML4, vm_tracker_t* vm_tracker = nullptr);
    
    void MapMemory(void* VirtualMemory, void* PhysicalMemory);
    void SetMapping(void* VirtualMemory, uint64_t value);
    void Unmap(void* VirtualMemory);

    void SetFlag(void* VirtualMemory, PT_Flag flag, bool status);
    bool GetFlag(void* VirtualMemory, PT_Flag flag);

    uint64_t getPhysicalAddress(void* virtualMemory);
    uint64_t getMapping(void* virtual_memory);
    void* PTM_ALLOCATE_PAGE();

    PageTable* PML4;
    vm_tracker_t* vm_tracker; // Where to mark the allocated memory (So we can free it up later on)
};

void ClonePTM(PageTableManager* dst, PageTableManager* src); // Clone a PTM

extern PageTableManager globalPTM;
extern "C" uint64_t global_ptm_cr3; // For use in assembly ISRs