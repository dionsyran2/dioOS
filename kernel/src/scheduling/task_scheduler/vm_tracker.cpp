/* Virtual Memory Tracker */
#include <scheduling/task_scheduler/vm_tracker.h>
#include <paging/PageFrameAllocator.h>
#include <paging/PageTableManager.h>



vm_struct* vm_tracker_t::get_page(uint64_t virtual_address) {
    vm_struct* vm = this->tree->floor_search(virtual_address);

    if (vm != nullptr) {
        if (virtual_address >= vm->start && virtual_address < (vm->start + vm->size)) {
            return vm;
        }
    }
    return nullptr;
}

vm_struct* vm_tracker_t::split_vma(vm_struct* vm, uint64_t split_addr) {
    // If the address is outside or exactly on the boundary, no split is needed
    if (split_addr <= vm->start || split_addr >= (vm->start + vm->size)) {
        return nullptr; 
    }

    // Create the new right-hand chunk
    vm_struct* new_vm = new vm_struct;
    new_vm->start = split_addr;
    new_vm->size = (vm->start + vm->size) - split_addr;
    new_vm->flags = vm->flags;

    // Shrink the original left-hand chunk
    vm->size = split_addr - vm->start;

    // Insert the new right-hand chunk into the tree
    this->tree->insert(new_vm->start, new_vm);

    return new_vm;
}

void vm_tracker_t::mark_allocation(uint64_t virtual_address, uint32_t size, uint32_t flags){
    vm_struct *vm = new vm_struct;
    vm->start = virtual_address;
    vm->size = size;
    vm->flags = flags;

    this->total_marked_memory += size;
    
    this->tree->insert(virtual_address, vm);
}

void vm_tracker_t::remove_allocation(uint64_t virtual_address, uint32_t size){
    uint64_t end_addr = virtual_address + size;
    uint64_t current_addr = virtual_address;

    while (current_addr < end_addr) {
        vm_struct* vm = get_page(current_addr);
        
        if (!vm) {
            current_addr += 0x1000; // Skip unmapped holes
            continue; 
        }

        if (current_addr > vm->start) {
            vm = split_vma(vm, current_addr);
        }

        if (end_addr < (vm->start + vm->size)) {
            split_vma(vm, end_addr);
        }

        // The chunk is now perfectly aligned with our removal request
        uint64_t next_addr = vm->start + vm->size;
        
        this->tree->remove(vm->start);
        delete vm;

        current_addr = next_addr;
    }

    this->total_marked_memory -= size;
}

// @brief Modifies flags, automatically splitting chunks if they partially overlap
void vm_tracker_t::set_flags(uint64_t virtual_address, uint32_t size, uint32_t new_flags) {
    uint64_t end_addr = virtual_address + size;
    uint64_t current_addr = virtual_address;

    while (current_addr < end_addr) {
        vm_struct* vm = get_page(current_addr);
        
        if (!vm) {
            current_addr += 0x1000; // Skip unmapped memory
            continue;
        }

        if (current_addr > vm->start) {
            vm = split_vma(vm, current_addr);
        }

        if (end_addr < (vm->start + vm->size)) {
            split_vma(vm, end_addr);
        }

        vm->flags = new_flags;

        current_addr += vm->size;
    }
}

uint8_t vm_tracker_t::get_flags(uint64_t virtual_address){
    vm_struct *vm = this->get_page(virtual_address);

    return vm ? vm->flags : 0;
}

void vm_tracker_t::free(PageTableManager* ptm){
    int new_ref = __atomic_sub_fetch(&this->reference_count, 1, __ATOMIC_SEQ_CST);

    if (new_ref > 0) return;

    kstd::linked_list_t<uint64_t> *entries = this->tree->range_search(0, UINT64_MAX);
    
    while (entries->size() > 0) {
        uint64_t virtual_address = entries->get(0);
        entries->remove(0);
        vm_struct *vm = this->tree->search(virtual_address);
        
        if (!vm) continue;

        uint64_t flags = vm->flags;
        uint64_t size = vm->size;
        delete vm;
        this->tree->remove(virtual_address);

        if (flags & VM_FLAG_DONT_FREE) continue;
        
        for (uint64_t offset = 0; offset < size; offset += 0x1000) {
            uint64_t physical = ptm->getPhysicalAddress((void*)(virtual_address + offset));
            if (physical){
                GlobalAllocator.DecreaseReferenceCount((void*)physical);
            }
        }
    }

    delete entries;
}

void vm_tracker_t::share_vm(){
    __atomic_add_fetch(&this->reference_count, 1, __ATOMIC_SEQ_CST);
}

void vm_tracker_t::copy_as_cow(PageTableManager* src, vm_tracker_t* child_tracker, PageTableManager* child_table){
    kstd::linked_list_t<uint64_t> *entries = this->tree->range_search(0, UINT64_MAX);
    
    while (entries->size() > 0) {
        uint64_t virtual_address = entries->get(0);
        entries->remove(0); // Pop the head off in O(1) time
        
        vm_struct *vm = this->get_page(virtual_address);
        if (!vm) continue;

        uint64_t flags = vm->flags;
        uint64_t size = vm->size;

        if (flags & VM_FLAG_DO_NOT_SHARE) continue;

        for (uint64_t offset = 0; offset < size; offset += 0x1000){
            uint64_t page = virtual_address + offset;
            uint64_t physical = src->getPhysicalAddress((void*)page);
            if (physical == 0) continue;
            
            if (flags & VM_FLAG_COW){
                flags |= VM_PENDING_COW;
                src->SetFlag((void*)page, PT_Flag::Write, false);
            }

            uint64_t raw = src->getMapping((void*)page);
            child_table->SetMapping((void*)page, raw);

            if (src->GetFlag((void*)page, User)){
                child_table->SetFlag((void*)page, User, true);
            }
            
            GlobalAllocator.IncreaseReferenceCount((void*)physical);
        }
        
        vm->flags = flags;
        child_tracker->mark_allocation(virtual_address, size, flags);
    }

    delete entries;
}

vm_tracker_t::vm_tracker_t(){
    this->tree = new kstd::avl_tree_t<vm_struct*>;
    this->reference_count = 1;
}
