#include <paging/PageTableManager.h>
#include <paging/PageMapIndexer.h>
#include <stdint.h>
#include <paging/PageFrameAllocator.h>
#include <memory.h>

PageTableManager globalPTM = NULL;

PageTableManager::PageTableManager(PageTable* PML4Address){
    this->PML4 = PML4Address;
}

void PageTableManager::ClonePTM(PageTableManager* ptm){
    for (int PDP_i = 0; PDP_i < 512; PDP_i++){
        auto* PDPEntry = &PML4->entries[PDP_i];
        if (!PDPEntry->GetFlag(PT_Flag::Present)) continue;

        auto* PDP = (PageTable*)PDPEntry->GetAddress();

        for (int PD_i = 0; PD_i < 512; PD_i++){
            auto* PDEntry = &PDP->entries[PD_i];
            if (!PDEntry->GetFlag(PT_Flag::Present)) continue;

            auto* PD = (PageTable*)PDEntry->GetAddress();

            for (int PT_i = 0; PT_i < 512; PT_i++){
                auto* PTEntry = &PD->entries[PT_i];
                if (!PTEntry->GetFlag(PT_Flag::Present)) continue;

                auto* PT = (PageTable*)PTEntry->GetAddress();

                uint64_t virt_base =
                    ((uint64_t)PDP_i << 39) |
                    ((uint64_t)PD_i  << 30) |
                    ((uint64_t)PT_i  << 21);

                for (int P_i = 0; P_i < 512; P_i++){
                    auto* P = &PT->entries[P_i];
                    if (!P->GetFlag(PT_Flag::Present)) continue;

                    uint64_t virtualAddress = virt_base | ((uint64_t)P_i << 12);

                    ptm->SetMemoryVal((void*)virtualAddress, P->Value);
                }
            }
        }
    }
}

void PageTableManager::SetMemoryVal(void* virtualMemory, uint64_t val){
    PageMapIndexer indexer = PageMapIndexer((uint64_t)virtualMemory);
    if (PML4 == nullptr) return;

    PageTable* PDP = nullptr; // Page Directory Pointer
    PageDirectoryEntry* PDPEntry = &PML4->entries[indexer.PDP_i];

    if (!PDPEntry->GetFlag(PT_Flag::Present)){
        PDP = (PageTable*)GlobalAllocator.RequestPage();
        //MapMemory(PDP, PDP);

        memset(PDP, 0, 0x1000);

        PDPEntry->SetAddress((uint64_t)PDP);
        PDPEntry->SetFlag(PT_Flag::Present, true);
        PDPEntry->SetFlag(PT_Flag::ReadWrite, true);
        PDPEntry->SetFlag(PT_Flag::UserSuper, true);
    }else{
        PDP = (PageTable*)PDPEntry->GetAddress();
    }

    PageTable* PD = nullptr;
    PageDirectoryEntry* PDEntry = &PDP->entries[indexer.PD_i];
    
    if (!PDEntry->GetFlag(PT_Flag::Present)){
        PD = (PageTable*)GlobalAllocator.RequestPage();
        //MapMemory(PD, PD);

        memset(PD, 0, 0x1000);

        PDEntry->SetAddress((uint64_t)PD);
        PDEntry->SetFlag(PT_Flag::Present, true);
        PDEntry->SetFlag(PT_Flag::ReadWrite, true);
        PDEntry->SetFlag(PT_Flag::UserSuper, true);
    }else{
        PD = (PageTable*)PDEntry->GetAddress();
    }

    PageTable* PT = nullptr;
    PageDirectoryEntry* PTEntry = &PD->entries[indexer.PT_i];
    
    if (!PTEntry->GetFlag(PT_Flag::Present)){
        PT = (PageTable*)GlobalAllocator.RequestPage();
        //MapMemory(PT, PT);

        memset(PT, 0, 0x1000);

        PTEntry->SetAddress((uint64_t)PT);
        PTEntry->SetFlag(PT_Flag::Present, true);
        PTEntry->SetFlag(PT_Flag::ReadWrite, true);
        PTEntry->SetFlag(PT_Flag::UserSuper, true);
    }else{
        PT = (PageTable*)PTEntry->GetAddress();
    }


    PageDirectoryEntry* PEntry = &PT->entries[indexer.P_i];

    PEntry->Value = val;

    __asm__ volatile("invlpg (%0)" ::"r"(virtualMemory) : "memory");
}

void PageTableManager::MapMemory(void* virtualMemory, void* physicalMemory){
    PageMapIndexer indexer = PageMapIndexer((uint64_t)virtualMemory);
    if (PML4 == nullptr) return;

    PageTable* PDP = nullptr; // Page Directory Pointer
    PageDirectoryEntry* PDPEntry = &PML4->entries[indexer.PDP_i];

    if (!PDPEntry->GetFlag(PT_Flag::Present)){
        PDP = (PageTable*)GlobalAllocator.RequestPage();
        //MapMemory(PDP, PDP);

        memset(PDP, 0, 0x1000);

        PDPEntry->SetAddress((uint64_t)PDP);
        PDPEntry->SetFlag(PT_Flag::Present, true);
        PDPEntry->SetFlag(PT_Flag::ReadWrite, true);
        PDPEntry->SetFlag(PT_Flag::UserSuper, true);
    }else{
        PDP = (PageTable*)PDPEntry->GetAddress();
    }

    PageTable* PD = nullptr;
    PageDirectoryEntry* PDEntry = &PDP->entries[indexer.PD_i];
    
    if (!PDEntry->GetFlag(PT_Flag::Present)){
        PD = (PageTable*)GlobalAllocator.RequestPage();
        //MapMemory(PD, PD);

        memset(PD, 0, 0x1000);

        PDEntry->SetAddress((uint64_t)PD);
        PDEntry->SetFlag(PT_Flag::Present, true);
        PDEntry->SetFlag(PT_Flag::ReadWrite, true);
        PDEntry->SetFlag(PT_Flag::UserSuper, true);
    }else{
        PD = (PageTable*)PDEntry->GetAddress();
    }

    PageTable* PT = nullptr;
    PageDirectoryEntry* PTEntry = &PD->entries[indexer.PT_i];
    
    if (!PTEntry->GetFlag(PT_Flag::Present)){
        PT = (PageTable*)GlobalAllocator.RequestPage();
        //MapMemory(PT, PT);

        memset(PT, 0, 0x1000);

        PTEntry->SetAddress((uint64_t)PT);
        PTEntry->SetFlag(PT_Flag::Present, true);
        PTEntry->SetFlag(PT_Flag::ReadWrite, true);
        PTEntry->SetFlag(PT_Flag::UserSuper, true);
    }else{
        PT = (PageTable*)PTEntry->GetAddress();
    }


    PageDirectoryEntry* PEntry = &PT->entries[indexer.P_i];

    PEntry->SetAddress((uint64_t)physicalMemory);
    PEntry->SetFlag(PT_Flag::Present, true);
    PEntry->SetFlag(PT_Flag::ReadWrite, true);
    PEntry->SetFlag(PT_Flag::UserSuper, true);

    __asm__ volatile("invlpg (%0)" ::"r"(virtualMemory) : "memory");
}

void PageTableManager::SetPageFlag(void* virtualMemory, PT_Flag flag, bool status){
    PageMapIndexer indexer = PageMapIndexer((uint64_t)virtualMemory);
    if (PML4 == nullptr) return;

    PageTable* PDP = nullptr; // Page Directory Pointer
    PageDirectoryEntry* PDPEntry = &PML4->entries[indexer.PDP_i];

    if (!PDPEntry->GetFlag(PT_Flag::Present)){
        PDP = (PageTable*)GlobalAllocator.RequestPage();
        MapMemory(PDP, PDP);

        memset(PDP, 0, 0x1000);

        PDPEntry->SetAddress((uint64_t)PDP);
        PDPEntry->SetFlag(PT_Flag::Present, true);
        PDPEntry->SetFlag(PT_Flag::ReadWrite, true);
        PDPEntry->SetFlag(PT_Flag::UserSuper, true);
    }else{
        PDP = (PageTable*)PDPEntry->GetAddress();
    }

    PageTable* PD = nullptr;
    PageDirectoryEntry* PDEntry = &PDP->entries[indexer.PD_i];
    
    if (!PDEntry->GetFlag(PT_Flag::Present)){
        PD = (PageTable*)GlobalAllocator.RequestPage();
        MapMemory(PD, PD);

        memset(PD, 0, 0x1000);

        PDEntry->SetAddress((uint64_t)PD);
        PDEntry->SetFlag(PT_Flag::Present, true);
        PDEntry->SetFlag(PT_Flag::ReadWrite, true);
        PDEntry->SetFlag(PT_Flag::UserSuper, true);
    }else{
        PD = (PageTable*)PDEntry->GetAddress();
    }

    PageTable* PT = nullptr;
    PageDirectoryEntry* PTEntry = &PD->entries[indexer.PT_i];
    
    if (!PTEntry->GetFlag(PT_Flag::Present)){
        PT = (PageTable*)GlobalAllocator.RequestPage();
        MapMemory(PT, PT);

        memset(PT, 0, 0x1000);

        PTEntry->SetAddress((uint64_t)PT);
        PTEntry->SetFlag(PT_Flag::Present, true);
        PTEntry->SetFlag(PT_Flag::ReadWrite, true);
        PTEntry->SetFlag(PT_Flag::UserSuper, true);
    }else{
        PT = (PageTable*)PTEntry->GetAddress();
    }

    PageDirectoryEntry* PEntry = &PT->entries[indexer.P_i];
    PEntry->SetFlag(flag, status);
    __asm__ volatile("invlpg (%0)" ::"r"(virtualMemory) : "memory");
}

void PageTableManager::UnmapMemory(void* virtualMemory){
    PageMapIndexer indexer = PageMapIndexer((uint64_t)virtualMemory);
    PageDirectoryEntry PDE;

    PDE = PML4->entries[indexer.PDP_i];
    PageTable* PDP;
    if (!PDE.GetFlag(PT_Flag::Present)) return;
    PDP = (PageTable*)((uint64_t)PDE.GetAddress());
    
    
    PDE = PDP->entries[indexer.PD_i];
    PageTable* PD;
    if (!PDE.GetFlag(PT_Flag::Present)) return;
    PD = (PageTable*)((uint64_t)PDE.GetAddress());
    

    PDE = PD->entries[indexer.PT_i];
    PageTable* PT;
    if (!PDE.GetFlag(PT_Flag::Present)) return;
    PT = (PageTable*)((uint64_t)PDE.GetAddress());
    

    PDE = PT->entries[indexer.P_i];
    PDE.SetFlag(PT_Flag::Present, false);
    PT->entries[indexer.P_i] = PDE;

    __asm__ volatile("invlpg (%0)" ::"r"(virtualMemory) : "memory");
}


bool PageTableManager::isMapped(void* vaddr){
    PageMapIndexer indexer = PageMapIndexer((uint64_t)vaddr);
    PageDirectoryEntry PDE;

    PDE = PML4->entries[indexer.PDP_i];
    PageTable* PDP;
    if (!PDE.GetFlag(PT_Flag::Present)) return false;
    PDP = (PageTable*)((uint64_t)PDE.GetAddress());
    
    
    PDE = PDP->entries[indexer.PD_i];
    PageTable* PD;
    if (!PDE.GetFlag(PT_Flag::Present)) return false;
    PD = (PageTable*)((uint64_t)PDE.GetAddress());
    

    PDE = PD->entries[indexer.PT_i];
    PageTable* PT;
    if (!PDE.GetFlag(PT_Flag::Present)) return false;
    PT = (PageTable*)((uint64_t)PDE.GetAddress());
    

    PDE = PT->entries[indexer.P_i];
    return PDE.GetFlag(PT_Flag::Present);
}

uint64_t PageTableManager::getPhysicalAddress(void* vaddr){
    PageMapIndexer indexer = PageMapIndexer((uint64_t)vaddr);
    PageDirectoryEntry PDE;

    PDE = PML4->entries[indexer.PDP_i];
    PageTable* PDP;
    if (!PDE.GetFlag(PT_Flag::Present)) return false;
    PDP = (PageTable*)((uint64_t)PDE.GetAddress());
    
    
    PDE = PDP->entries[indexer.PD_i];
    PageTable* PD;
    if (!PDE.GetFlag(PT_Flag::Present)) return false;
    PD = (PageTable*)((uint64_t)PDE.GetAddress());
    

    PDE = PD->entries[indexer.PT_i];
    PageTable* PT;
    if (!PDE.GetFlag(PT_Flag::Present)) return false;
    PT = (PageTable*)((uint64_t)PDE.GetAddress());
    

    PDE = PT->entries[indexer.P_i];
    return PDE.GetFlag(PT_Flag::Present) ? PDE.GetAddress() : NULL;
}