#include <memory/vmm.h>
#include <paging/PageFrameAllocator.h>
#include <kerrno.h>


mm_struct_t::mm_struct_t(){
    // Start by creating the page tables
    PageTable *PML4 = (PageTable*)GlobalAllocator.RequestPage();
    this->_page_table_manager = new PageTableManager(PML4);
    memset(PML4, 0, PAGE_SIZE);

    // A bit 'hacky', copy the kernel tables (They are shared)
    for (int i = 256; i < 512; i++) {
        PML4->entries[i] = globalPTM.PML4->entries[i];
    }
    
}

mm_struct_t::~mm_struct_t(){
    // Undo everything we did in the constructor.
    // Unallocate any user page tables and free the page table manager
    
    this->_page_table_manager->destroyUserMappings();
    GlobalAllocator.FreePage(this->_page_table_manager->PML4);
    delete this->_page_table_manager;
    this->_page_table_manager = nullptr;

}

uint64_t mm_struct_t::_find_unmapped_area(uint64_t length) {
    uint64_t current_search = this->_mmap_hint_address;
    if (current_search == 0) current_search = DEFAULT_VM_MMAP_BASE;

    current_search = ALIGN(current_search, PAGE_SIZE);
    length = ALIGN(length, PAGE_SIZE);

    __m_area_t *current = this->_m_area_list;

    // What if the list is empty? The whole space is free!
    if (current == nullptr) return current_search;

    // Check the gap BEFORE the first VMA
    if (current_search < current->start) {
        if ((current->start - current_search) >= length) {
            return current_search;
        }
    }

    // Traverse the list looking for gaps between consecutive VMAs
    while (current != nullptr) {
        // Force the search pointer to the end of the current block
        if (current_search < (current->start + current->size)) {
            current_search = current->start + current->size;
        }

        // If there is a next block, check the gap between 'current' and 'next'
        if (current->next != nullptr) {
            uint64_t gap = current->next->start - current_search;
            if (gap >= length) {
                return current_search;
            }
        }
        current = current->next;
    }

    // Checked all gaps between VMAs. Place it after the very last VMA.
    // Ensure we don't overflow into kernel space (0x800000000000)
    if (current_search + length < 0x800000000000) {
        return current_search;
    }

    return 0; // Out of memory
}

bool mm_struct_t::is_free(uint64_t start, uint64_t size){
    __m_area_t *current = this->_m_area_list;

    uint64_t end = start + size;
    while (current != nullptr){
        uint64_t current_end = current->start + current->size;

        if (current->start <= start && current->start > current_end) return false;
        if (current->start < end && current_end > end) return false;
    }

    return true;
}

void mm_struct_t::_insert_segment(uint64_t start, uint64_t size, uint64_t flags){
    __m_area_t *new_vma = new __m_area_t();
    new_vma->start = start;
    new_vma->size = size;
    new_vma->flags = flags;
    new_vma->next = nullptr;
    new_vma->previous = nullptr;

    if (this->_m_area_list == nullptr){
        this->_m_area_list = new_vma;
        return;
    }

    __m_area_t *current = this->_m_area_list;
    __m_area_t *prev = nullptr;

    while (current != nullptr && current->start < start){
        prev = current;
        current = current->next;
    }

    if (prev == nullptr){
        // Insert at the head
        new_vma->next = this->_m_area_list;
        this->_m_area_list->previous = new_vma;
        this->_m_area_list = new_vma;
    } else {
        // Insert somewhere in the middle / end
        new_vma->next = current;
        new_vma->previous = prev;
        prev->next = new_vma;
        if (current) current->previous = new_vma;
    }

    this->_merge_vmas();
    return;
}

void mm_struct_t::_split_vma(__m_area_t* vma, uint64_t split_address){
    if (split_address <= vma->start || split_address >= (vma->start + vma->size)) return;

    // Create the right-hand half
    __m_area_t *new_right = new __m_area_t();
    new_right->start = split_address;
    new_right->size = (vma->start + vma->size) - split_address;
    new_right->flags = vma->flags;

    // Modify the left-hand half
    vma->size = split_address - vma->start;

    // Insert the new_right into the list, right after vma
    new_right->next = vma->next;
    new_right->previous = vma;
    if (vma->next) vma->next->previous = new_right;
    vma->next = new_right;
}

void mm_struct_t::_remove_segment(uint64_t start, uint64_t size){
    uint64_t end = start + size;
    __m_area_t *current = this->_m_area_list;

    while (current != nullptr){
        uint64_t vm_end = current->start + current->size;
        // If we are past the end of the range, stop
        if (current->start >= end) break;

        // Check for overlap
        if (start < vm_end && end > current->start) {
            // If it starts in the middle of the vma split it
            if (start > current->start){
                this->_split_vma(current, start);
                current = current->next; // Move to the right half
                continue; // Re-evaluate the overlap on the right half
            }

            // Does it end in the middle of this vma? split it.
            if (end < vm_end){
                this->_split_vma(current, end);
            }

            // At this point the vma is entirely inside the [start, end] range.
            // lets free the physical pages
            for (uint64_t addr = current->start; addr < vm_end; addr += PAGE_SIZE) {
                uint64_t phys = this->_page_table_manager->getPhysicalAddress((void*)addr);
                if (phys != 0) {
                    GlobalAllocator.FreePage((void*)phys);
                    
                    this->_page_table_manager->Unmap((void*)addr); 
                }
            }

            // Remove the vma from the list
            __m_area_t *to_delete = current;
            if (to_delete->previous){
                to_delete->previous->next = to_delete->next;
            } else {
                this->_m_area_list = to_delete->next;
            }

            if (to_delete->next) to_delete->next->previous = to_delete->previous;

            current = current->next;
            delete to_delete;
        } else {
            current = current->next;
        }
    }
}

void mm_struct_t::_merge_vmas(){
    __m_area_t *current = this->_m_area_list;

    while (current != nullptr && current->next != nullptr){
        __m_area_t *next = current->next;

        // If they touch and have the same permissions
        if ((current->start + current->size) == next->start && current->flags == next->flags) {
            // Absorb the next one into the current one
            current->size += next->size;

            current->next = next->next;
            if (next->next) next->next->previous = current;

            delete next;
        } else {
            current = current->next;
        }
    }
}





void *mm_struct_t::allocate(uint64_t start, uint64_t size, uint64_t flags, int &errno){
    start = ALIGN_DOWN(start, PAGE_SIZE);
    size = ALIGN(size, PAGE_SIZE);

    uint64_t irq_flags = spin_lock(&this->_lock);

    if (!this->is_free(start, size)){
        errno = EADDRINUSE; // Not the correct thing to return... but i don't know anything more fitting.
        return nullptr;
    }

    this->_insert_segment(start, size, flags);
    
    for (size_t offset = 0; offset < size; offset += 0x1000) {
        void *page = GlobalAllocator.RequestPage();

        uint64_t physical = virtual_to_physical((uint64_t)page);
        this->_page_table_manager->MapMemory((void*)(start + offset), (void*)physical, flags & 0xfff0000000000fff /* transfer only the PTM flags */);
    }

    spin_unlock(&this->_lock, irq_flags);
    
    errno = 0;

    return (void *)start;
}

void *mm_struct_t::allocate(uint64_t size, uint64_t flags, int &errno){
    size = ALIGN(size, PAGE_SIZE);

    uint64_t irq_flags = spin_lock(&this->_lock);

    uint64_t start = this->_find_unmapped_area(size);
    
    this->_insert_segment(start, size, flags);
    
    for (size_t offset = 0; offset < size; offset += 0x1000) {
        void *page = GlobalAllocator.RequestPage();

        uint64_t physical = virtual_to_physical((uint64_t)page);
        this->_page_table_manager->MapMemory((void*)(start + offset), (void*)physical, flags & 0xfff0000000000fff /* transfer only the PTM flags */);
    }

    spin_unlock(&this->_lock, irq_flags);

    errno = 0;
    return (void *)start;
}

void mm_struct_t::free(uint64_t start, uint64_t size, int &errno){
    start = ALIGN_DOWN(start, PAGE_SIZE);
    size = ALIGN(size, PAGE_SIZE);

    uint64_t irq_flags = spin_lock(&this->_lock);

    this->_remove_segment(start, size);
    
    spin_unlock(&this->_lock, irq_flags);
    errno = 0;
}

void mm_struct_t::destroy_address_space(){
    while (this->_m_area_list != nullptr){
        this->_remove_segment(this->_m_area_list->start, this->_m_area_list->size);
    }
}

uint64_t mm_struct_t::get_vm_size(){
    return this->_vm_size;
}

// Shared vm state
void mm_struct_t::open(){
    __atomic_add_fetch(&this->_reference_count, 1, __ATOMIC_SEQ_CST);
}

void mm_struct_t::close(){
    int count = __atomic_sub_fetch(&this->_reference_count, 1, __ATOMIC_SEQ_CST);

    if (count <= 0) {
        this->destroy_address_space();
        delete this;
    }
}