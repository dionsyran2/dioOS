#include <paging/PageFrameAllocator.h>
#include <kernel.h>

uint64_t freeMemory;
uint64_t reservedMemory;
uint64_t usedMemory;
uint64_t memorySize;
uint64_t lowestFreeSeg = NULL;
size_t lowestFreeSegSize = 0; 
bool Initialized = false;
PageFrameAllocator GlobalAllocator;

void PageFrameAllocator::ReadEFIMemoryMap(limine_memmap_response* mmap){
    if (Initialized) return;
    Initialized = true;

    uint64_t largestFreeMemSeg = NULL;
    size_t largestFreeMemSegSize = 0;
    

    for (int i = 0; i < mmap->entry_count; i++){
        limine_memmap_entry* desc = (limine_memmap_entry*)mmap->entries[i];
        if (desc->type == LIMINE_MEMMAP_USABLE && (uint64_t)desc->base < 0x40000000){ // type = EfiConventionalMemory
            if (lowestFreeSeg > desc->base || lowestFreeSeg == 0){
                lowestFreeSeg = desc->base;
                lowestFreeSegSize = desc->length;
            }

            if (desc->length > largestFreeMemSegSize)
            {
                largestFreeMemSeg = desc->base;
                largestFreeMemSegSize = desc->length;
            }
        }
    }


    if (largestFreeMemSeg == lowestFreeSeg){
        largestFreeMemSeg += 0x1000;
    }

    memorySize = GetMemorySize(mmap);
    freeMemory = memorySize;

    uint64_t bitmapSize = memorySize / 4096 / 8 + 1;
    InitBitmap(bitmapSize, (void*)physical_to_virtual(largestFreeMemSeg));
 
    ReservePages(0, (memorySize / 0x1000) + 1);
    for (int i = 0; i < mmap->entry_count; i++){
        limine_memmap_entry* desc = (limine_memmap_entry*)mmap->entries[i];
        if (desc->type == LIMINE_MEMMAP_USABLE){
            UnreservePages((void*)desc->base, desc->length / 0x1000);
        }
    }
    ReservePages(0, 0x100); // reserve between 0 and 0x100000
    ReservePage((void*)lowestFreeSeg);
    LockPages(PageBitmap.Buffer, (PageBitmap.Size / 4096) + 1);
}

void PageFrameAllocator::InitBitmap(size_t bitmapSize, void* bufferAddress){
    PageBitmap.Size = bitmapSize;
    PageBitmap.Buffer = (uint8_t*)bufferAddress;
    memset(PageBitmap.Buffer, 0, bitmapSize);
}
uint64_t pageBitmapIndex = 0;
void* PageFrameAllocator::RequestPage(){

    for (; pageBitmapIndex < PageBitmap.Size * 8; pageBitmapIndex++){
        if (PageBitmap[pageBitmapIndex] == true) continue;
        LockPage((void*)(pageBitmapIndex * 4096));

        return (void*)physical_to_virtual((pageBitmapIndex * 4096));
    }

    if (pageBitmapIndex == PageBitmap.Size * 8){
        pageBitmapIndex = 0;
    }

    return NULL; // Page Frame Swap to file
}

void* PageFrameAllocator::RequestPages(uint32_t pages){
    
    uint64_t pb = pageBitmapIndex;
    for (; pb < PageBitmap.Size * 8; pb++){
        if (PageBitmap[pb] == true) continue;
        bool found = true;
        for (int i=0; i < pages; i++){//check if there are enough consecutive pages
            if (PageBitmap[pb+i] == true){
                found = false;
                break;
            }
        }
        if (found == false) continue;
        for (int i=0; i < pages; i++){
            LockPage((void*)((pb + i) * 4096));
        }

        return (void*)physical_to_virtual((pb * 4096));
    }

    return NULL; //Not enough consecutive pages
}


void PageFrameAllocator::FreePage(void* address){
    if ((uint64_t)address >= MEMORY_BASE) address = (void*)virtual_to_physical((uint64_t)address); 
    uint64_t index = (uint64_t)address / 4096;
    if (PageBitmap[index] == false) return;


    if (PageBitmap.Set(index, false)){
        freeMemory += 4096;
        usedMemory -= 4096;
        if (pageBitmapIndex > index) pageBitmapIndex = index;
    }
}

void PageFrameAllocator::FreePages(void* address, uint64_t pageCount){
    if ((uint64_t)address >= MEMORY_BASE) address = (void*)virtual_to_physical((uint64_t)address); 
    for (int t = 0; t < pageCount; t++){
        FreePage((void*)((uint64_t)address + (t * 4096)));
    }
}

void PageFrameAllocator::LockPage(void* address){
    if ((uint64_t)address >= MEMORY_BASE) address = (void*)virtual_to_physical((uint64_t)address); 
    uint64_t index = (uint64_t)address / 4096;
    if (PageBitmap[index] == true) return;
    if (PageBitmap.Set(index, true)){
        freeMemory -= 4096;
        usedMemory += 4096;
    }
}

void PageFrameAllocator::LockPages(void* address, uint64_t pageCount){
    if ((uint64_t)address >= MEMORY_BASE) address = (void*)virtual_to_physical((uint64_t)address); 
    for (int t = 0; t < pageCount; t++){
        LockPage((void*)((uint64_t)address + (t * 4096)));
    }
}

void PageFrameAllocator::UnreservePage(void* address){
    if ((uint64_t)address >= MEMORY_BASE) address = (void*)virtual_to_physical((uint64_t)address); 
    uint64_t index = (uint64_t)address / 4096;
    if (PageBitmap[index] == false) return;
    if (PageBitmap.Set(index, false)){
        freeMemory += 4096;
        reservedMemory -= 4096;
        if (pageBitmapIndex > index) pageBitmapIndex = index;
    }
}

void PageFrameAllocator::UnreservePages(void* address, uint64_t pageCount){
    if ((uint64_t)address >= MEMORY_BASE) address = (void*)virtual_to_physical((uint64_t)address); 
    for (int t = 0; t < pageCount; t++){
        UnreservePage((void*)((uint64_t)address + (t * 4096)));
    }
}

void PageFrameAllocator::ReservePage(void* address){
    if ((uint64_t)address >= MEMORY_BASE) address = (void*)virtual_to_physical((uint64_t)address); 
    uint64_t index = (uint64_t)address / 4096;
    if (PageBitmap[index] == true) return;
    if (PageBitmap.Set(index, true)){
        freeMemory -= 4096;
        reservedMemory += 4096;
    }
}

void PageFrameAllocator::ReservePages(void* address, uint64_t pageCount){
    if ((uint64_t)address >= MEMORY_BASE) address = (void*)virtual_to_physical((uint64_t)address); 
    for (int t = 0; t < pageCount; t++){
        ReservePage((void*)((uint64_t)address + (t * 4096)));
    }
}

void* PageFrameAllocator::GetPageAboveBase(uint64_t base){
    if (base >= MEMORY_BASE) base = virtual_to_physical(base); 

    void* page = RequestPage();
    if ((uint64_t)page > base){
        return page;
    }
    void* pg = GetPageAboveBase(base);
    FreePage(page);
    return (void*)physical_to_virtual((uint64_t)pg);
    
}
uint64_t PageFrameAllocator::GetFreeRAM(){
    return freeMemory;
}
uint64_t PageFrameAllocator::GetUsedRAM(){
    return usedMemory;
}
uint64_t PageFrameAllocator::GetReservedRAM(){
    return reservedMemory;
}

uint64_t PageFrameAllocator::GetMemSize(){
    return memorySize;
}