#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/spinlock/spinlock.h>

#define VM_FLAG_RW (1 << 0)
#define VM_FLAG_US (1 << 1)
#define VM_FLAG_WT (1 << 2)
#define VM_FLAG_CD (1 << 3)
#define VM_FLAG_NX (1 << 4)
#define VM_FLAG_DO_NOT_SHARE (1 << 5) // Used for Page table entries (each task has its own ptm)
#define VM_FLAG_COW (1 << 6)
#define VM_PENDING_COW (1 << 7)
#define VM_FLAG_DONT_FREE (1 << 8) // It will not free this area unless the 2nd argument is set when exit is called


struct vm_struct{
    uint64_t address;
    uint32_t size;
    uint32_t flags;

    vm_struct* next;
};

struct PageTableManager;

class vm_tracker_t{ // Virtual memory manager
    public:
    vm_struct* vm_list;

    uint64_t total_marked_memory;
    spinlock_t lock;

    void mark_allocation(uint64_t virtual_address, uint32_t size, uint32_t flags);
    void remove_allocation(uint64_t virtual_address, uint32_t size);
    void change_flags(uint64_t virtual_address, uint32_t size, uint32_t new_flags);
    uint8_t get_flags(uint64_t virtual_address);
    vm_struct* get_page(uint64_t virtual_address);
    void exit(void* ptmv, bool force = false);
    
    private:
    void add_to_list(vm_struct* vm);
    void remove_from_list(vm_struct* vm);
    vm_struct* _handle_trimming(vm_struct* target, uint64_t unmap_start, uint64_t unmap_end);
};

void copy_vmm(vm_tracker_t* dest, vm_tracker_t* src);