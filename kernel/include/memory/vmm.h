#pragma once
#include <stdint.h>
#include <stddef.h>
#include <paging/PageTableManager.h>

#define DEFAULT_VM_MMAP_BASE 0x40000000

struct __m_area_t{
    uint64_t start;
    uint64_t size;
    uint64_t flags;

    __m_area_t *next;
    __m_area_t *previous;
};

struct PageTableManager;
class mm_struct_t{
    public:
    mm_struct_t();
    ~mm_struct_t();

    uint64_t resolve_physical_address(uint64_t virt);
    uint64_t get_root_page_table();
    
    void *allocate(uint64_t start, uint64_t size, uint64_t flags, int &errno);
    void *allocate(uint64_t size, uint64_t flags, int &errno);

    void free(uint64_t start, uint64_t size, int &errno);
    void destroy_address_space();

    bool is_free(uint64_t start, uint64_t size);

    uint64_t get_vm_size();

    // Shared vm state
    void open();
    void close();

    private:
    void _insert_segment(uint64_t start, uint64_t size, uint64_t flags);
    void _remove_segment(uint64_t start, uint64_t size);
    uint64_t _find_unmapped_area(uint64_t size);

    void _merge_vmas();
    void _split_vma(__m_area_t* vma, uint64_t split_address);

    private:
    __m_area_t *_m_area_list = nullptr;
    PageTableManager *_page_table_manager;

    uint64_t _mmap_hint_address = 0;

    uint64_t _vm_size = 0;

    int _reference_count = 0;

    spinlock_t _lock;
};