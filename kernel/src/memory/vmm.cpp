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

    _reference_count = 1;
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

    if (current == nullptr) return current_search;

    // Fast-forward past any VMAs that end BEFORE our search base (e.g. executable code at 0x400000)
    while (current != nullptr && (current->start + current->size) <= current_search) {
        current = current->next;
    }

    // Check gap before the first VMA above DEFAULT_VM_MMAP_BASE
    if (current != nullptr && current_search < current->start) {
        if ((current->start - current_search) >= length) {
            return current_search;
        }
    }

    // Traverse gaps between consecutive VMAs
    while (current != nullptr) {
        if (current_search < (current->start + current->size)) {
            current_search = current->start + current->size;
        }

        if (current->next != nullptr) {
            if (current->next->start > current_search) {
                uint64_t gap = current->next->start - current_search;
                if (gap >= length) {
                    return current_search;
                }
            }
        }
        current = current->next;
    }

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

        // The golden rule of range overlap:
        if (start < current_end && current->start < end) {
            return false; // Collision detected!
        }

        // Advance to the next VMA in the list!
        current = current->next;
    }

    return true;
}

__m_area_t* mm_struct_t::find_vma(uint64_t address) {
    __m_area_t *current = this->_m_area_list;
    while (current != nullptr) {
        if (address >= current->start && address < (current->start + current->size)) {
            return current;
        }
        current = current->next;
    }
    return nullptr;
}

// Make sure to update your header file to match this signature!
// void _insert_segment(uint64_t start, uint64_t size, uint64_t flags, vnode_t *file = nullptr, uint64_t file_offset = 0);

void mm_struct_t::_insert_segment(uint64_t start, uint64_t size, uint64_t flags, vnode_t *file, uint64_t file_offset) {
    __m_area_t *new_vma = new __m_area_t();
    new_vma->start = start;
    new_vma->size = size;
    new_vma->flags = flags;
    
    // --- NEW: File Backing ---
    new_vma->file = file;
    new_vma->file_offset = file_offset;
    
    if (new_vma->file) new_vma->file->open();

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
        new_vma->next = this->_m_area_list;
        this->_m_area_list->previous = new_vma;
        this->_m_area_list = new_vma;
    } else {
        new_vma->next = current;
        new_vma->previous = prev;
        prev->next = new_vma;
        if (current) current->previous = new_vma;
    }

    this->_merge_vmas();
}

void mm_struct_t::_split_vma(__m_area_t* vma, uint64_t split_address) {
    if (split_address <= vma->start || split_address >= (vma->start + vma->size)) return;

    __m_area_t *new_right = new __m_area_t();
    new_right->start = split_address;
    new_right->size = (vma->start + vma->size) - split_address;
    new_right->flags = vma->flags;

    new_right->file = vma->file;
    new_right->file_offset = vma->file_offset + (split_address - vma->start);
    
    if (new_right->file) new_right->file->open(); 

    // Modify the left-hand half
    vma->size = split_address - vma->start;

    // List wiring
    new_right->next = vma->next;
    new_right->previous = vma;
    if (vma->next) vma->next->previous = new_right;
    vma->next = new_right;
}

void mm_struct_t::_remove_segment(uint64_t start, uint64_t size) {
    uint64_t end = start + size;
    __m_area_t *current = this->_m_area_list;

    // Unmap the physical memory right away for the target area
    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
        uint64_t phys = this->_page_table_manager->getPhysicalAddress((void*)addr);
        if (phys != 0) {
            GlobalAllocator.DecreaseReferenceCount((void*)phys);
            this->_page_table_manager->Unmap((void*)addr);
        }
    }

    while (current != nullptr) {
        uint64_t vm_end = current->start + current->size;
        __m_area_t *next_node = current->next; // Save next before modifying the list!

        // If this VMA is completely outside our range, skip it
        if (vm_end <= start || current->start >= end) {
            current = next_node;
            continue;
        }

        // VMA is completely engulfed by the removal area. Delete it entirely.
        if (current->start >= start && vm_end <= end) {
            if (current->previous) current->previous->next = current->next;
            else this->_m_area_list = current->next;
            
            if (current->next) current->next->previous = current->previous;
            
            if (current->file) current->file->close();
            delete current;
        }
        // VMA encompasses the removal area entirely (middle split)
        else if (current->start < start && vm_end > end) {
            this->_split_vma(current, start);      // Splits into [current->start, start] and [start, vm_end]
            this->_split_vma(current->next, end);  // Splits the right half into [start, end] and [end, vm_end]
            
            // Delete the middle chunk we just isolated
            __m_area_t *middle = current->next;
            middle->previous->next = middle->next;
            if (middle->next) middle->next->previous = middle->previous;
            
            if (middle->file) middle->file->close();
            delete middle;
        }
        // Removal overlaps the right edge of the VMA
        else if (current->start < start && vm_end <= end) {
            current->size = start - current->start;
        }
        // Removal overlaps the left edge of the VMA
        else if (current->start >= start && vm_end > end) {
            uint64_t overlap = end - current->start;
            current->start = end;
            current->size -= overlap;
            if (current->file) current->file_offset += overlap;
        }

        current = next_node; // Safely move to the next node
    }
}

void mm_struct_t::_merge_vmas() {
    __m_area_t *current = this->_m_area_list;

    while (current != nullptr && current->next != nullptr){
        __m_area_t *next = current->next;

        // Check if they touch in memory and have the same permissions
        bool memory_contiguous = ((current->start + current->size) == next->start);
        bool same_flags = (current->flags == next->flags);
        
        // Check if they map the same file
        bool same_file = (current->file == next->file);
        
        // If they are file-backed, check if they are contiguous
        bool file_contiguous = true; 
        if (current->file != nullptr) {
            file_contiguous = ((current->file_offset + current->size) == next->file_offset);
        }

        // If ALL conditions are met, merge them!
        if (memory_contiguous && same_flags && same_file && file_contiguous) {
            current->size += next->size;

            current->next = next->next;
            if (next->next) next->next->previous = current;

            if (next->file) next->file->close(); // Drop the duplicate reference before deleting

            delete next;
        } else {
            current = current->next;
        }
    }
}
uint64_t mm_struct_t::resolve_physical_address(uint64_t virt){
    return this->_page_table_manager->getPhysicalAddress((void*)virt);
}

uint64_t mm_struct_t::get_root_page_table(){
    return virtual_to_physical((uint64_t)this->_page_table_manager->PML4);
}


void *mm_struct_t::mmap(uint64_t start, uint64_t size, uint64_t vm_flags, vnode_t* file, uint64_t file_offset, int &errno) {
    size = ALIGN(size, PAGE_SIZE);
    
    uint64_t irq_flags = spin_lock(&this->_lock);

    if (vm_flags & VM_FIXED) {
        if (start == 0 || (start % PAGE_SIZE != 0)) {
            spin_unlock(&this->_lock, irq_flags);
            errno = EINVAL;
            return nullptr;
        }

        if (!this->is_free(start, size)) {
            this->_remove_segment(start, size);
        }
    } else {
        start = ALIGN_DOWN(start, PAGE_SIZE);

        if (start == 0 || !this->is_free(start, size)) {
            start = this->_find_unmapped_area(size);
            if (start == 0) {
                spin_unlock(&this->_lock, irq_flags);
                errno = ENOMEM;
                return nullptr;
            }
        }
    }

    this->_insert_segment(start, size, vm_flags, file, file_offset);
    
    spin_unlock(&this->_lock, irq_flags);
    errno = 0;
    return (void *)start;
}

void *mm_struct_t::mmap(uint64_t size, uint64_t flags, vnode_t* file, uint64_t file_offset, int &errno){
    return mmap(0, size, flags, file, file_offset, errno);
}


int mm_struct_t::mprotect(uint64_t start, uint64_t size, uint64_t prot_flags) {
    start = ALIGN_DOWN(start, PAGE_SIZE);
    size = ALIGN(size, PAGE_SIZE);
    uint64_t end = start + size;

    uint64_t irq_flags = spin_lock(&this->_lock);
    
    __m_area_t *current = this->_m_area_list;

    while (current != nullptr) {
        uint64_t vm_end = current->start + current->size;
        
        if (current->start < start && vm_end > start) {
            this->_split_vma(current, start);
        }
        
        if (current->start < end && vm_end > end) {
            this->_split_vma(current, end);
        }
        
        current = current->next;
    }

    // Apply protections
    current = this->_m_area_list;
    while (current != nullptr) {
        // If this VMA is inside our target range
        if (current->start >= start && (current->start + current->size) <= end) {
            
            // Clear old protections, apply the new ones
            current->flags = (current->flags & ~(VM_READ | VM_WRITE | VM_EXEC)) | prot_flags;
            
            // Re-map hardware page tables to reflect new flags!
            for (uint64_t addr = current->start; addr < current->start + current->size; addr += PAGE_SIZE) {
                uint64_t phys = this->_page_table_manager->getPhysicalAddress((void*)addr);
                
                if (phys != 0) { // If it's already paged in
                    uint64_t pt_flags = (1ULL << PT_Flag::User) | (1ULL << PT_Flag::Present);
                    if (current->flags & VM_WRITE) pt_flags |= (1ULL << PT_Flag::Write);
                    if (!(current->flags & VM_EXEC)) pt_flags |= (1ULL << PT_Flag::NX);
                    
                    this->_page_table_manager->MapMemory((void*)addr, (void*)phys, pt_flags);
                }
            }
        }
        current = current->next;
    }

    // Merge any VMAs that now share identical flags and boundaries!
    this->_merge_vmas(); 
    
    spin_unlock(&this->_lock, irq_flags);

    return 0;
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

bool mm_struct_t::handle_page_fault(uint64_t address, uint64_t error_code, bool kernel_override){
    task_t *self = task_scheduler::get_current_task();
    if (self->saved_fpu_state) save_fpu_state(self->saved_fpu_state);

    address &= ~0xFFFUL;
    __m_area_t *vma = this->find_vma(address);

    if (vma == nullptr) return false;

    bool is_write_fault = error_code & 0x2;
    bool is_exec_fault  = error_code & 0x10;

    // Reject writes to read-only memory
    if (is_write_fault && !(vma->flags & VM_WRITE) && !kernel_override) {
        return false;
    }

    // Reject execution in non-executable memory
    if (is_exec_fault && !(vma->flags & VM_EXEC)) {
        return false; 
    }

    // Translate VM flags to Hardware Page Table Flags
    uint64_t pt_flags = (1ULL << PT_Flag::User) | (1ULL << PT_Flag::Present);
    if (vma->flags & VM_WRITE) pt_flags |= (1ULL << PT_Flag::Write);
    if (!(vma->flags & VM_EXEC)) pt_flags |= (1ULL << PT_Flag::NX);

    uint64_t existing_phys = this->_page_table_manager->getPhysicalAddress((void*)address);
    if (existing_phys != 0) {
        // Update the flags?
        this->_page_table_manager->MapMemory((void*)address, (void*)existing_phys, pt_flags);
        
        if (self->saved_fpu_state) restore_fpu_state(self->saved_fpu_state);
        return true;
    }

    // Demand Allocate the Physical Page
    void* page = GlobalAllocator.RequestPage();
    memset(page, 0, PAGE_SIZE);

    if (vma->file != nullptr) {
        uint64_t page_offset_in_vma = address - vma->start;
        uint64_t exact_file_offset = vma->file_offset + page_offset_in_vma;
        uint64_t file_size = vma->file->size;
        
        if (exact_file_offset < file_size) {
            uint64_t bytes_to_read = min((uint64_t)PAGE_SIZE, file_size - exact_file_offset);
            vma->file->read(page, bytes_to_read, exact_file_offset);
        }
    }

    this->_page_table_manager->MapMemory((void*)address, (void*)virtual_to_physical((uint64_t)page), pt_flags);
    
    // Always flush the TLB when introducing a new mapping

    if (self->saved_fpu_state) restore_fpu_state(self->saved_fpu_state);
    return true;
}