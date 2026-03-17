/* Page Frame Allocation */
#pragma once

#include <stdint.h>
#include <stddef.h>

#include <limine.h>
#include <scheduling/spinlock/spinlock.h>

#define PAGE_SIZE 0x1000
#define PAGE_FLAGS_LOCKED (1 << 0)

struct page_struct{
    uint8_t reference_count;
    uint8_t flags;
    spinlock_t spinlock;
};

class PageFrameAllocator{
    public:

    bool Initialize(limine_memmap_response* memmap);
    bool LockPage(void* page);
    bool LockPages(void* page, size_t amount);

    bool UnlockPage(void* page);
    bool UnlockPages(void* page, size_t amount);

    bool ReservePage(void* page);
    bool ReservePages(void* page, size_t amount);

    bool UnreservePage(void* page);
    bool UnreservePages(void* page, size_t amount);

    bool IsLocked(uint64_t address);

    void* RequestPage();
    void* RequestPages(size_t amount);

    void FreePage(void* page);
    void FreePages(void* page, size_t amount);

    int GetReferenceCount(uint64_t page);
    bool SetReferenceCount(uint64_t page, int ref_cnt);
    void IncreaseReferenceCount(void* page);
    void DecreaseReferenceCount(void* page);


    uint64_t total_memory;
    uint64_t free_memory;
    uint64_t used_memory;
    uint64_t reserved_memory;

    private:
    uint64_t total_pages;

    uint64_t last_free_index;

    page_struct* page_table;

    spinlock_t spinlock;
};

extern PageFrameAllocator GlobalAllocator; 