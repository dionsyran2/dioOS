#pragma once
#include <stdint.h>
#include <stddef.h>

#include <paging/PageEntry.h>
#include <scheduling/task_scheduler/task_scheduler.h>


class PageTableManager{
    public:
    PageTableManager(PageTable* PML4);
    
    void MapMemory(void* VirtualMemory, void* PhysicalMemory);
    void MapMemory(void* VirtualMemory, void* PhysicalMemory, uint64_t flags);
    void SetMapping(void* VirtualMemory, uint64_t value);
    void Unmap(void* VirtualMemory);

    void SetFlag(void* VirtualMemory, PT_Flag flag, bool status);
    bool GetFlag(void* VirtualMemory, PT_Flag flag);

    uint64_t getPhysicalAddress(void* virtualMemory);
    uint64_t getMapping(void* virtual_memory);

    void destroyUserMappings();
    void _free_table_recursive(PageTable* table, int level);

    PageTable* PML4;
};

extern PageTableManager globalPTM;
extern "C" uint64_t global_ptm_cr3; // For use in assembly ISRs