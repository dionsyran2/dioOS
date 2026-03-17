/* Virtual Memory Tracker */
#include <scheduling/task_scheduler/vm_tracker.h>
#include <paging/PageFrameAllocator.h>
#include <paging/PageTableManager.h>

void vm_tracker_t::add_to_list(vm_struct* vm){
    if (vm_list == nullptr || vm->address < vm_list->address) {
        vm->next = vm_list;
        vm_list = vm;
        return;
    }

    vm_struct* v = vm_list;
    while (v->next != nullptr && v->next->address < vm->address) {
        v = v->next;
    }
    
    vm->next = v->next;
    v->next = vm;
}

void vm_tracker_t::remove_from_list(vm_struct* target){
    vm_struct* prev = nullptr;

    for (vm_struct* vm = vm_list; vm != nullptr; vm = vm->next){
        if (vm == target){
            if (prev == nullptr) {
                vm_list = vm->next;
            }else{
                prev->next = vm->next;
            }
            break;
        }
        
        prev = vm;
    }
}


void vm_tracker_t::mark_allocation(uint64_t virtual_address, uint32_t size, uint32_t flags){
    total_marked_memory += size;


    for (vm_struct* vm = vm_list; vm != nullptr; vm = vm->next){
        bool at_end = (vm->address + vm->size) == virtual_address;
        bool at_start = (virtual_address + size) == vm->address;
        bool extends = (at_end || at_start) && vm->flags == flags;

        if (!extends) continue;

        if (at_start){
            vm->address = virtual_address;
            vm->size += size;
        } else {
            vm->size += size;

            if (vm->next && vm->next->address == (vm->address + vm->size) && vm->next->flags == flags) {
                vm_struct* victim = vm->next;
                vm->size += victim->size;
                vm->next = victim->next;
                delete victim;
            }
        }
        return;
    }

    vm_struct* vm = new vm_struct;
    vm->address = virtual_address;
    vm->size = size;
    vm->flags = flags;
    vm->next = nullptr;

    add_to_list(vm);
}

vm_struct* vm_tracker_t::_handle_trimming(vm_struct* target, uint64_t start, uint64_t end){
    uint64_t vma_start = target->address;
    uint64_t vma_end = vma_start + target->size;
    // Partial overlap / Split cases!
    // Check if it leaves a prefix, a suffix, or both
    
    // Leaves a prefix
    if (start > vma_start) {
        target->size = start - vma_start;

        if (end >= vma_end) {
            return target->next; 
        }
    }

    // Leaves a suffix
    if (end < vma_end) {
        if (start <= vma_start) {
            target->address = end;
            target->size = vma_end - end;
        } else {
            vm_struct* suffix = new vm_struct;

            suffix->address = end;
            suffix->size = vma_end - end;
            suffix->flags = target->flags;

            suffix->next = target->next;
            target->next = suffix;
            return suffix->next;
        }
    }
    
    return target->next;
}

void vm_tracker_t::remove_allocation(uint64_t virtual_address, uint32_t size){
    uint64_t start = virtual_address;
    uint64_t end = start + size;

    for (vm_struct* vm = vm_list; vm != nullptr;){
        uint64_t vma_start = vm->address;
        uint64_t vma_end = vma_start + vm->size;

        bool full_overlap = (vma_start >= start && vma_end <= end);
        bool overlaps = !(end <= vma_start || start >= vma_end);
        bool trim = overlaps && !(vma_start >= start && vma_end <= end);
        
        if (full_overlap){
            vm_struct* next = vm->next;

            remove_from_list(vm);
            delete vm;

            vm = next;
            continue;
        }else if (trim){
            vm = _handle_trimming(vm, start, end);
            continue;
        }

        vm = vm->next;
    }

    total_marked_memory -= size;
}
void vm_tracker_t::change_flags(uint64_t start, uint32_t size, uint32_t new_flags) {
    uint64_t end = start + size;

    for (vm_struct* vm = vm_list; vm != nullptr;){
        uint64_t vma_start = vm->address;
        uint64_t vma_end = vma_start + vm->size;

        bool full_overlap = (vma_start >= start && vma_end <= end);
        bool overlaps = !(end <= vma_start || start >= vma_end);
        bool trim = overlaps && !full_overlap;

        if (full_overlap){
            vm_struct* next = vm->next;
            remove_from_list(vm);
            delete vm;
            vm = next;
            continue;
        } else if (trim){
            vm = _handle_trimming(vm, start, end);
            continue;
        }

        vm = vm->next;
    }

    mark_allocation(start, size, new_flags);
}

uint8_t vm_tracker_t::get_flags(uint64_t virtual_address){
    for (vm_struct* vm = vm_list; vm != nullptr; vm = vm->next){
        if (virtual_address >= vm->address && virtual_address < (vm->address + vm->size))
            return vm->flags;
    }

    return 0;
}

#include <kstdio.h>
vm_struct* vm_tracker_t::get_page(uint64_t virtual_address){
    for (vm_struct* vm = vm_list; vm != nullptr; vm = vm->next){        
        if (virtual_address >= vm->address && virtual_address < (vm->address + vm->size)){
            return vm;
        }
    }
    return nullptr;
}

void vm_tracker_t::exit(void* ptmv, bool force){
    PageTableManager* ptm = (PageTableManager*)ptmv;
    if (!ptm) return;
    for (vm_struct* vm = vm_list; vm != nullptr;){
        if (!force && vm->flags & VM_FLAG_DONT_FREE){
            vm = vm->next;
            continue;
        }
        
        for (int i = 0; i < vm->size; i += 0x1000){
            GlobalAllocator.DecreaseReferenceCount((void*)ptm->getPhysicalAddress((void*)(vm->address + i)));
        }

        vm_struct* nvm = vm->next;

        remove_from_list(vm);
        
        delete vm;
        vm = nvm;
    }
    
    vm_list = nullptr;
}

void copy_vmm(vm_tracker_t* dest, vm_tracker_t* src){
    for (vm_struct* vm = src->vm_list; vm != nullptr; vm = vm->next){
        if (vm->flags & VM_FLAG_DO_NOT_SHARE) continue;
        dest->mark_allocation(vm->address, vm->size, vm->flags);
    }
}