#pragma once
#include "paging.h"

class PageTableManager {
    public:
    PageTableManager(PageTable* PML4Address);
    PageTable* PML4;
    void MapMemory(void* virtualMemory, void* physicalMemory);
    void SetPageFlag(void* virtualMemory, PT_Flag flag, bool status);
    void UnmapMemory(void* virtualMemory);
};

extern PageTableManager globalPTM;
