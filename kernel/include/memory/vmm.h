#pragma once
#include <stdint.h>
#include <stddef.h>
#include <paging/PageTableManager.h>

#define DEFAULT_VM_MMAP_BASE 0x40000000

#define VM_READ       0x01
#define VM_WRITE      0x02
#define VM_EXEC       0x04
#define VM_SHARED     0x08
#define VM_ANON       0x10
#define VM_FIXED      0x20

struct vnode_t;

struct __m_area_t{
    uint64_t start;
    uint64_t size;
    uint64_t flags;

    // --- File Mapping Info ---
    vnode_t* file;         // Pointer to the file (nullptr if VM_ANON)
    uint64_t file_offset;  // Where in the file this memory region starts

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
    
    void *mmap(uint64_t start, uint64_t size, uint64_t vm_flags, vnode_t* file, uint64_t file_offset, int &errno);
    void *mmap(uint64_t size, uint64_t vm_flags, vnode_t* file, uint64_t file_offset, int &errno);
    int mprotect(uint64_t start, uint64_t size, uint64_t prot_flags);

    void free(uint64_t start, uint64_t size, int &errno);
    void destroy_address_space();

    bool is_free(uint64_t start, uint64_t size);
    __m_area_t* find_vma(uint64_t address);

    uint64_t get_vm_size();

    mm_struct_t* fork();

    bool handle_page_fault(uint64_t address, uint64_t error_code, bool kernel_override = false);

    // Shared vm state
    void open();
    void close();
    PageTableManager *_page_table_manager;


    private:
    void _insert_segment(uint64_t start, uint64_t size, uint64_t flags, vnode_t *file, uint64_t file_offset);
    void _remove_segment(uint64_t start, uint64_t size);
    uint64_t _find_unmapped_area(uint64_t size);

    void _merge_vmas();
    void _split_vma(__m_area_t* vma, uint64_t split_address);


    private:
    __m_area_t *_m_area_list = nullptr;

    uint64_t _mmap_hint_address = 0;

    uint64_t _vm_size = 0;

    int _reference_count = 0;

    spinlock_t _lock;

    public:
    uint64_t initial_brk;
    uint64_t current_brk;
};