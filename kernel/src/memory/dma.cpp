#include <memory/dma.h>
#include <ArrayList.h>
#include <memory/heap.h>
#include <paging/PageTableManager.h>

struct dma_allocation{
    uint64_t physical_address;
    size_t size;
};


ArrayList<dma_allocation*>* allocations = nullptr;

void* dma_alloc(size_t size, size_t alignment, size_t boundary){
    if (allocations == nullptr) allocations = new ArrayList<dma_allocation*>();

    size_t size_in_pages = (size + 0xFFF) / 0x1000;

    void* physical_address = GlobalAllocator.RequestPages(size_in_pages);
    uintptr_t start = (uintptr_t)physical_address;
    uintptr_t end = start + size;

    if (boundary && ((start & ~(boundary - 1)) != ((start + size - 1) & ~(boundary - 1)))) {
        return dma_alloc(size, alignment, boundary);
    }

    dma_allocation* allocation = new dma_allocation;
    allocation->physical_address = start;
    allocation->size = size;
    allocations->add(allocation);

    return physical_address;
}

uint64_t dma_phys_addr(void* vaddr){
    return (uint64_t)globalPTM.getPhysicalAddress(vaddr);
}

void dma_free(void* addr){
    if (allocations == nullptr) return;

    for (size_t i = 0; i < allocations->length(); i++){
        dma_allocation* alloc = allocations->get(i);
        if (alloc->physical_address == dma_phys_addr(addr)){
            allocations->remove(alloc);
            GlobalAllocator.FreePages((void*)alloc->physical_address, alloc->size / 0x1000 + 1);
            delete alloc;
            break;
        }
    }
}