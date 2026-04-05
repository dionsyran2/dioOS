#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/spinlock/spinlock.h>
#include <paging/PageTableManager.h>
#include <structures/trees/avl_tree.h>

#define VM_FLAG_RW (1 << 0)
#define VM_FLAG_US (1 << 1)
#define VM_FLAG_WT (1 << 2)
#define VM_FLAG_CD (1 << 3)
#define VM_FLAG_NX (1 << 4)
#define VM_FLAG_DO_NOT_SHARE (1 << 5) // Used for Page table entries (each task has its own ptm)
#define VM_FLAG_COW (1 << 6)
#define VM_PENDING_COW (1 << 7)
#define VM_FLAG_DONT_FREE (1 << 8) // It will not free this area unless the 2nd argument is set when exit is called
#define VM_FLAG_SHARED (1 << 9)


struct vm_struct {
    uint64_t start;
    uint64_t size;
    uint16_t flags;
};

struct PageTableManager;

class vm_tracker_t{ // Virtual memory manager
    public:
    vm_struct* vm_list = nullptr;

    uint64_t total_marked_memory = 0;
    spinlock_t lock = 0;

    void mark_allocation(uint64_t virtual_address, uint32_t size, uint32_t flags);
    void remove_allocation(uint64_t virtual_address, uint32_t size);
    void set_flags(uint64_t virtual_address, uint32_t size, uint32_t new_flags);
    uint8_t get_flags(uint64_t virtual_address);
    void free(PageTableManager* ptm);
    void share_vm();
    void copy_as_cow(PageTableManager* src, vm_tracker_t* child_tracker, PageTableManager* child_table);
    
    vm_tracker_t();

    private:
    vm_struct* get_page(uint64_t virtual_address);
    vm_struct* split_vma(vm_struct* vm, uint64_t split_addr);

    public:
    uint64_t brk_offset = 0;
    uint64_t rmap_offset = 0;
    
    private:
    kstd::avl_tree_t<vm_struct*> *tree;
    int reference_count;
};