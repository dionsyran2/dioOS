#include <drivers/USB/xhci_mem.h>
#include <paging/PageTableManager.h>
#include <memory/dma.h>
#include <BasicRenderer.h>

uintptr_t xhci_map_mmio(uint64_t pci_bar_address, uint32_t bar_size){
    size_t page_count = bar_size / PAGE_SIZE;

    for (int i = 0; i < page_count; i++){
        void* address = (void*)(pci_bar_address + (PAGE_SIZE * i));
        globalPTM.MapMemory(address, address);
        globalPTM.SetPageFlag(address, PT_Flag::CacheDisabled, true);
        globalPTM.SetPageFlag(address, PT_Flag::UserSuper, false);
    }
    return (uintptr_t)pci_bar_address;
}

void* alloc_xhci_memory(size_t size, size_t alignment, size_t boundary){
    if (size == 0 || alignment == 0 || boundary == 0){
        return nullptr;
    }
    
    void* block = dma_alloc(size, alignment, boundary);
    if (block == 0){
        kprintf("Invalid allocation | XHCI");
        while(1);
    }

    memset(block, 0, size);

    return block;
}

void free_xhci_memory(void* ptr){
    dma_free(ptr);
}

uintptr_t xhci_get_physical_addr(void* vaddr){
    return dma_phys_addr(vaddr);
}