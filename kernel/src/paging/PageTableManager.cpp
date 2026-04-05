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
PageTableManager::PageTableManager(PageTable* PML4, vm_tracker_t* vm_tracker){
    this->PML4 = PML4;
    this->vm_tracker = vm_tracker;
}

void ClonePTM(PageTableManager* dst, PageTableManager* src){
    // Copy the Kernel Mappings
    memcpy(&dst->PML4->entries[256], &src->PML4->entries[256], sizeof(PageEntry[256]));

    if (!dst->vm_tracker || !src->vm_tracker) return;

    src->vm_tracker->copy_as_cow(src, dst->vm_tracker, dst);
}

// A wrapper to allocate a page
void* PageTableManager::PTM_ALLOCATE_PAGE(){
    void* page = GlobalAllocator.RequestPage();
    if (vm_tracker != nullptr) vm_tracker->mark_allocation((uint64_t)page, PAGE_SIZE, VM_FLAG_RW | VM_FLAG_DO_NOT_SHARE);

    return page;
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
        void* virt = PTM_ALLOCATE_PAGE();
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
        void* virt = PTM_ALLOCATE_PAGE();
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
        void* virt = PTM_ALLOCATE_PAGE();
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
    page->set_flag(PT_Flag::Write, true);
    
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
        void* virt = PTM_ALLOCATE_PAGE();
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
        void* virt = PTM_ALLOCATE_PAGE();
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
        void* virt = PTM_ALLOCATE_PAGE();
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