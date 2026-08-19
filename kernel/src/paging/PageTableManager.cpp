#include <paging/PageTableManager.h>
#include <paging/PageFrameAllocator.h>
#include <paging/PageMapIndexer.h>
#include <kernel.h>
#include <memory.h>
#include <cpu.h>

/* Global PTM Definition */
PageTableManager globalPTM = nullptr;
uint64_t global_ptm_cr3 = 0;
/* Constructor */
PageTableManager::PageTableManager(PageTable* PML4){
    this->PML4 = PML4;
}

void PageTableManager::MapMemory(void* VirtualMemory, void* PhysicalMemory, uint64_t flags){
    this->MapMemory(VirtualMemory, PhysicalMemory);

    int bit = __builtin_ffsll(flags) - 1;

    while (flags != 0) {
        flags &= ~(1UL << bit);

        this->SetFlag(VirtualMemory, (PT_Flag)bit, true);
        bit = __builtin_ffsll(flags) - 1;
    }
}

/* Maps Virtual -> Physical */
void PageTableManager::MapMemory(void* VirtualMemory, void* PhysicalMemory){    
    /* Get the index */
    PageMapIndexer index = PageMapIndexer((uint64_t)VirtualMemory);

    /* --- Level 4 (PML4) --- */
    PageTable* L4 = this->PML4;
    PageEntry* L3_Entry = &L4->entries[index.L3_i];

    PageTable* L3; // PDPT
    if (!L3_Entry->is_flag_set(PT_Flag::Present)){
        /* Allocate a L3 Table (PDPT) */
        void* virt = GlobalAllocator.RequestPage();
        uint64_t physical = virtual_to_physical((uint64_t)virt);

        memset(virt, 0, PAGE_SIZE);

        L3_Entry->set_address(physical);
        L3_Entry->set_flag(PT_Flag::Present, true);
        L3_Entry->set_flag(PT_Flag::Write, true);
        
        L3 = (PageTable*)virt;
    } else {
        L3 = (PageTable*)physical_to_virtual(L3_Entry->get_address());
    }

    /* --- Level 3 (PDPT) --- */
    PageEntry* L2_Entry = &L3->entries[index.L2_i];

    PageTable* L2; // PD
    if (!L2_Entry->is_flag_set(PT_Flag::Present)){
        /* Allocate a L2 Table (PD) */
        void* virt = GlobalAllocator.RequestPage();
        uint64_t physical = virtual_to_physical((uint64_t)virt);

        memset(virt, 0, PAGE_SIZE);

        L2_Entry->set_address(physical);
        L2_Entry->set_flag(PT_Flag::Present, true);
        L2_Entry->set_flag(PT_Flag::Write, true);
        
        L2 = (PageTable*)virt;
    } else {
        L2 = (PageTable*)physical_to_virtual(L2_Entry->get_address());
    }

    /* --- Level 2 (PD) --- */
    PageEntry* L1_Entry = &L2->entries[index.L1_i]; // Get pointer to L2 entry

    PageTable* L1; // PT
    if (!L1_Entry->is_flag_set(PT_Flag::Present)){
        /* Allocate a L1 Table (PT) */
        void* virt = GlobalAllocator.RequestPage();
        uint64_t physical = virtual_to_physical((uint64_t)virt);

        memset(virt, 0, PAGE_SIZE);

        L1_Entry->set_address(physical);
        L1_Entry->set_flag(PT_Flag::Present, true);
        L1_Entry->set_flag(PT_Flag::Write, true);
        
        L1 = (PageTable*)virt;
    } else {
        L1 = (PageTable*)physical_to_virtual(L1_Entry->get_address());
    }

    /* --- Level 1 (PT) --- */
    PageEntry* page = &L1->entries[index.P_i];

    page->value = 0; // Clear the entry before writing new values

    page->set_address((uint64_t)PhysicalMemory);
    page->set_flag(PT_Flag::Present, true);
    
    /* Flush the TLB for the single virtual address */
    __native_flush_tlb_single((uint64_t)VirtualMemory);
}

void PageTableManager::SetMapping(void* VirtualMemory, uint64_t value){
    /* Get the index */
    PageMapIndexer index = PageMapIndexer((uint64_t)VirtualMemory);

    /* --- Level 4 (PML4) --- */
    PageTable* L4 = this->PML4;
    PageEntry* L3_Entry = &L4->entries[index.L3_i];

    PageTable* L3; // PDPT
    if (!L3_Entry->is_flag_set(PT_Flag::Present)){
        /* Allocate a L3 Table (PDPT) */
        void* virt = GlobalAllocator.RequestPage();
        uint64_t physical = virtual_to_physical((uint64_t)virt);

        memset(virt, 0, PAGE_SIZE);

        L3_Entry->set_address(physical);
        L3_Entry->set_flag(PT_Flag::Present, true);
        L3_Entry->set_flag(PT_Flag::Write, true);
        
        L3 = (PageTable*)virt;
    } else {
        L3 = (PageTable*)physical_to_virtual(L3_Entry->get_address());
    }

    /* --- Level 3 (PDPT) --- */
    PageEntry* L2_Entry = &L3->entries[index.L2_i];

    PageTable* L2; // PD
    if (!L2_Entry->is_flag_set(PT_Flag::Present)){
        /* Allocate a L2 Table (PD) */
        void* virt = GlobalAllocator.RequestPage();
        uint64_t physical = virtual_to_physical((uint64_t)virt);

        memset(virt, 0, PAGE_SIZE);

        L2_Entry->set_address(physical);
        L2_Entry->set_flag(PT_Flag::Present, true);
        L2_Entry->set_flag(PT_Flag::Write, true);
        
        L2 = (PageTable*)virt;
    } else {
        L2 = (PageTable*)physical_to_virtual(L2_Entry->get_address());
    }

    /* --- Level 2 (PD) --- */
    PageEntry* L1_Entry = &L2->entries[index.L1_i]; // Get pointer to L2 entry

    PageTable* L1; // PT
    if (!L1_Entry->is_flag_set(PT_Flag::Present)){
        /* Allocate a L1 Table (PT) */
        void* virt = GlobalAllocator.RequestPage();
        uint64_t physical = virtual_to_physical((uint64_t)virt);

        memset(virt, 0, PAGE_SIZE);

        L1_Entry->set_address(physical);
        L1_Entry->set_flag(PT_Flag::Present, true);
        L1_Entry->set_flag(PT_Flag::Write, true);
        
        L1 = (PageTable*)virt;
    } else {
        L1 = (PageTable*)physical_to_virtual(L1_Entry->get_address());
    }

    /* --- Level 1 (PT) --- */
    PageEntry* page = &L1->entries[index.P_i];

    page->value = value;
    
    /* Flush the TLB for the single virtual address */
    __native_flush_tlb_single((uint64_t)VirtualMemory);
}


void PageTableManager::Unmap(void* VirtualMemory){
    /* Get the indexer */
    PageMapIndexer index = PageMapIndexer((uint64_t)VirtualMemory);

    /* Get the L4 table */
    PageTable* L4 = this->PML4;

    /* Get the L3 table*/
    PageEntry L3_Entry = L4->entries[index.L3_i];

    PageTable* L3 = (PageTable*)physical_to_virtual(L3_Entry.get_address());
    if (!L3_Entry.is_flag_set(PT_Flag::Present)) return;

    /* Get the L2 table */
    PageEntry L2_Entry = L3->entries[index.L2_i];

    PageTable* L2 = (PageTable*)physical_to_virtual(L2_Entry.get_address());
    if (!L2_Entry.is_flag_set(PT_Flag::Present)) return;

    /* Get the L1 table */
    PageEntry L1_Entry = L2->entries[index.L1_i];

    PageTable* L1 = (PageTable*)physical_to_virtual(L1_Entry.get_address());
    if (!L1_Entry.is_flag_set(PT_Flag::Present)) return;

    /* Get the final Page Entry (PTE) */
    PageEntry* page = &L1->entries[index.P_i];

    if (!page->is_flag_set(PT_Flag::Present)) return;
    
    /* Unmap the page */
    page->set_flag(PT_Flag::Present, false); // Clear Present bit

    /* Flush */
    __native_flush_tlb_single((uint64_t)VirtualMemory);
}
void PageTableManager::SetFlag(void* VirtualMemory, PT_Flag flag, bool status){
    PageMapIndexer index = PageMapIndexer((uint64_t)VirtualMemory);
    PageTable* L4 = this->PML4;

    // --- LEVEL 4 ---
    PageEntry* L4_Entry = &L4->entries[index.L3_i]; // Get pointer to modify it
    if (!L4_Entry->is_flag_set(PT_Flag::Present)) return;

    if (flag == PT_Flag::User && status) {
        L4_Entry->set_flag(PT_Flag::User, true);
    }

    // --- LEVEL 3 ---
    PageTable* L3 = (PageTable*)physical_to_virtual(L4_Entry->get_address());
    PageEntry* L3_Entry = &L3->entries[index.L2_i];
    if (!L3_Entry->is_flag_set(PT_Flag::Present)) return;

    if (flag == PT_Flag::User && status) {
        L3_Entry->set_flag(PT_Flag::User, true);
    }

    // --- LEVEL 2 ---
    PageTable* L2 = (PageTable*)physical_to_virtual(L3_Entry->get_address());
    PageEntry* L2_Entry = &L2->entries[index.L1_i];
    if (!L2_Entry->is_flag_set(PT_Flag::Present)) return;

    if (flag == PT_Flag::User && status) {
        L2_Entry->set_flag(PT_Flag::User, true);
    }

    // --- LEVEL 1 (Final Page) ---
    PageTable* L1 = (PageTable*)physical_to_virtual(L2_Entry->get_address());
    PageEntry* page = &L1->entries[index.P_i];
    
    if (!page->is_flag_set(PT_Flag::Present)) return;
    
    // Set the flag on the leaf node
    page->set_flag(flag, status);

    __native_flush_tlb_single((uint64_t)VirtualMemory);
}

bool PageTableManager::GetFlag(void* VirtualMemory, PT_Flag flag){
    /* Get the indexer */
    PageMapIndexer index = PageMapIndexer((uint64_t)VirtualMemory);

    /* Get the L4 table */
    PageTable* L4 = this->PML4;

    /* Get the L3 table*/
    PageEntry L3_Entry = L4->entries[index.L3_i];

    PageTable* L3 = (PageTable*)physical_to_virtual(L3_Entry.get_address());
    if (!L3_Entry.is_flag_set(PT_Flag::Present)) return false;

    /* Get the L2 table */
    PageEntry L2_Entry = L3->entries[index.L2_i];

    PageTable* L2 = (PageTable*)physical_to_virtual(L2_Entry.get_address());
    if (!L2_Entry.is_flag_set(PT_Flag::Present)) return false;

    /* Get the L1 table */
    PageEntry L1_Entry = L2->entries[index.L1_i];

    PageTable* L1 = (PageTable*)physical_to_virtual(L1_Entry.get_address());
    if (!L1_Entry.is_flag_set(PT_Flag::Present)) return false;

    /* Get the final Page Entry (PTE) */
    PageEntry L0_Entry = L1->entries[index.P_i];

    if (!L0_Entry.is_flag_set(PT_Flag::Present)) return false;
    
    /* Get the flag */
    PageEntry* page = &L1->entries[index.P_i]; // Point directly to the PTE in the L1 table
    return page->is_flag_set(flag);
}

uint64_t PageTableManager::getPhysicalAddress(void* VirtualMemory){
    /* Get the indexer */
    PageMapIndexer index = PageMapIndexer((uint64_t)VirtualMemory);

    /* Get the L4 table */
    PageTable* L4 = this->PML4;

    /* Get the L3 table*/
    PageEntry L3_Entry = L4->entries[index.L3_i];

    PageTable* L3 = (PageTable*)physical_to_virtual(L3_Entry.get_address());
    if (!L3_Entry.is_flag_set(PT_Flag::Present)) return false;

    /* Get the L2 table */
    PageEntry L2_Entry = L3->entries[index.L2_i];

    PageTable* L2 = (PageTable*)physical_to_virtual(L2_Entry.get_address());
    if (!L2_Entry.is_flag_set(PT_Flag::Present)) return false;

    /* Get the L1 table */
    PageEntry L1_Entry = L2->entries[index.L1_i];

    PageTable* L1 = (PageTable*)physical_to_virtual(L1_Entry.get_address());
    if (!L1_Entry.is_flag_set(PT_Flag::Present)) return false;

    /* Get the final Page Entry (PTE) */
    PageEntry L0_Entry = L1->entries[index.P_i];

    if (!L0_Entry.is_flag_set(PT_Flag::Present)) return false;
    
    /* Get the flag */
    PageEntry* page = &L1->entries[index.P_i]; // Point directly to the PTE in the L1 table
    return page->get_address();
}


uint64_t PageTableManager::getMapping(void* VirtualMemory){
    /* Get the indexer */
    PageMapIndexer index = PageMapIndexer((uint64_t)VirtualMemory);

    /* Get the L4 table */
    PageTable* L4 = this->PML4;

    /* Get the L3 table*/
    PageEntry L3_Entry = L4->entries[index.L3_i];

    PageTable* L3 = (PageTable*)physical_to_virtual(L3_Entry.get_address());
    if (!L3_Entry.is_flag_set(PT_Flag::Present)) return false;

    /* Get the L2 table */
    PageEntry L2_Entry = L3->entries[index.L2_i];

    PageTable* L2 = (PageTable*)physical_to_virtual(L2_Entry.get_address());
    if (!L2_Entry.is_flag_set(PT_Flag::Present)) return false;

    /* Get the L1 table */
    PageEntry L1_Entry = L2->entries[index.L1_i];

    PageTable* L1 = (PageTable*)physical_to_virtual(L1_Entry.get_address());
    if (!L1_Entry.is_flag_set(PT_Flag::Present)) return false;

    /* Get the final Page Entry (PTE) */
    PageEntry L0_Entry = L1->entries[index.P_i];

    if (!L0_Entry.is_flag_set(PT_Flag::Present)) return false;
    
    /* Get the flag */
    PageEntry* page = &L1->entries[index.P_i]; // Point directly to the PTE in the L1 table
    return page->value;
}

// Inside PageTableManager.cpp
void PageTableManager::destroyUserMappings() {
    if (!this->PML4) return;

    for (int i = 0; i < 256; i++) {
        PageEntry entry = this->PML4->entries[i];
        
        if (entry.is_flag_set(PT_Flag::Present)) {
            uint64_t phys_addr = entry.get_address();
            PageTable* pdpt = (PageTable*)physical_to_virtual(phys_addr);
            
            // Recurse to Level 3 (PDPT)
            _free_table_recursive(pdpt, 3);
        }
    }
}

void PageTableManager::_free_table_recursive(PageTable* table, int level) {
    // If we reach Level 1 (PT), the tables below it are the physical data frames.
    // The VMA `_remove_segment` already freed the data frames! 
    // We only free the routing tables here.
    if (level == 1) {
        GlobalAllocator.FreePage((void*)virtual_to_physical((uint64_t)table));
        return;
    }

    // For Level 3 (PDPT) and Level 2 (PD), recurse downward.
    for (int i = 0; i < 512; i++) {
        PageEntry entry = table->entries[i];
        
        if (entry.is_flag_set(PT_Flag::Present)) {
            // Check for huge pages (2MB/1GB). They point directly to data, not to lower tables.
            bool is_huge_page = ((level == 2 || level == 3) && entry.is_flag_set(PT_Flag::LargerPages));

            if (!is_huge_page) {
                uint64_t phys_addr = entry.get_address();
                PageTable* next_table = (PageTable*)physical_to_virtual(phys_addr);
                _free_table_recursive(next_table, level - 1);
            }
        }
    }

    // After all children are freed, free this table
    GlobalAllocator.FreePage((void*)virtual_to_physical((uint64_t)table));
}