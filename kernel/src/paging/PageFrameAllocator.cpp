#include <paging/PageFrameAllocator.h>
#include <scheduling/lock/spinlock.h>

uint64_t freeMemory;
uint64_t reservedMemory;
uint64_t usedMemory;
uint64_t memorySize;
void* lowestFreeSeg = NULL;
size_t lowestFreeSegSize = 0; 
bool Initialized = false;
PageFrameAllocator GlobalAllocator;

void PageFrameAllocator::ReadEFIMemoryMap(EFI_MEMORY_DESCRIPTOR* mMap, size_t mMapSize, size_t mMapDescSize){
    if (Initialized) return;

    Initialized = true;

    size_t mMapEntries = (mMapSize - sizeof(multiboot_tag_efi_mmap)) / mMapDescSize;

    void* largestFreeMemSeg = NULL;
    size_t largestFreeMemSegSize = 0;
    void* secLargestFreeMemSeg = NULL;
    size_t secLargestFreeMemSegSize = 0;
    

    for (int i = 0; i < mMapEntries; i++){
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)mMap + (i * mMapDescSize));
        if (desc->type == 7 && (uint64_t)desc->physAddr < 0x40000000){ // type = EfiConventionalMemory
            if (lowestFreeSeg > desc->physAddr || lowestFreeSeg == NULL){
                lowestFreeSeg = desc->physAddr;
                lowestFreeSegSize = desc->numPages * 0x1000;
            }

            if (desc->numPages * 4096 > largestFreeMemSegSize)
            {
                secLargestFreeMemSeg = largestFreeMemSeg;
                secLargestFreeMemSegSize = largestFreeMemSegSize;
                largestFreeMemSeg = desc->physAddr;
                largestFreeMemSegSize = desc->numPages * 4096;
            }
        }
        /*if ((desc->type == 7 || desc->type == 3 || desc->type == 4 || desc->type == 1 || desc->type == 2) && (uint64_t)desc->physAddr < 0x40000000){
            if (desc->physAddr < lowestMemLoc && ((uint64_t)desc->physAddr < 0x100000)){
                lowestMemLoc = desc->physAddr;
            }
        }*/
    }
    /*if (largestFreeMemSeg == lowestMemLoc){
        largestFreeMemSeg = secLargestFreeMemSeg;
        largestFreeMemSegSize = secLargestFreeMemSegSize;
    }*/

    if (largestFreeMemSeg == lowestFreeSeg){
        largestFreeMemSeg += 0x1000;
    }

    memorySize = GetMemorySize(mMap, mMapEntries, mMapDescSize);
    freeMemory = memorySize;
    uint64_t bitmapSize = memorySize / 4096 / 8 + 1;
    InitBitmap(bitmapSize, largestFreeMemSeg);

    ReservePages(0, (memorySize / 0x1000) + 1);
    for (int i = 0; i < mMapEntries; i++){
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)mMap + (i * mMapDescSize));
        if (desc->type == 7){ // efiConventionalMemory
            UnreservePages(desc->physAddr, desc->numPages);
        }
    }
    //ReservePages(0, 0x100); // reserve between 0 and 0x100000
    LockPage(lowestFreeSeg);
    LockPages(PageBitmap.Buffer, PageBitmap.Size / 4096 + 1);
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

        return (void*)(pageBitmapIndex * 4096);
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

        return (void*)(pb * 4096);
    }

    return NULL; //Not enough consecutive pages
}


void PageFrameAllocator::FreePage(void* address){
    uint64_t index = (uint64_t)address / 4096;
    if (PageBitmap[index] == false) return;


    if (PageBitmap.Set(index, false)){
        freeMemory += 4096;
        usedMemory -= 4096;
        if (pageBitmapIndex > index) pageBitmapIndex = index;
    }
}

void PageFrameAllocator::FreePages(void* address, uint64_t pageCount){
    for (int t = 0; t < pageCount; t++){
        FreePage((void*)((uint64_t)address + (t * 4096)));
    }
}

void PageFrameAllocator::LockPage(void* address){
    uint64_t index = (uint64_t)address / 4096;
    if (PageBitmap[index] == true) return;
    if (PageBitmap.Set(index, true)){
        freeMemory -= 4096;
        usedMemory += 4096;
    }
}

void PageFrameAllocator::LockPages(void* address, uint64_t pageCount){
    for (int t = 0; t < pageCount; t++){
        LockPage((void*)((uint64_t)address + (t * 4096)));
    }
}

void PageFrameAllocator::UnreservePage(void* address){
    uint64_t index = (uint64_t)address / 4096;
    if (PageBitmap[index] == false) return;
    if (PageBitmap.Set(index, false)){
        freeMemory += 4096;
        reservedMemory -= 4096;
        if (pageBitmapIndex > index) pageBitmapIndex = index;
    }
}

void PageFrameAllocator::UnreservePages(void* address, uint64_t pageCount){
    for (int t = 0; t < pageCount; t++){
        UnreservePage((void*)((uint64_t)address + (t * 4096)));
    }
}

void PageFrameAllocator::ReservePage(void* address){
    uint64_t index = (uint64_t)address / 4096;
    if (PageBitmap[index] == true) return;
    if (PageBitmap.Set(index, true)){
        freeMemory -= 4096;
        reservedMemory += 4096;
    }
}

void PageFrameAllocator::ReservePages(void* address, uint64_t pageCount){
    for (int t = 0; t < pageCount; t++){
        ReservePage((void*)((uint64_t)address + (t * 4096)));
    }
}

void* PageFrameAllocator::GetPageAboveBase(uint64_t base){
    void* page = RequestPage();
    if ((uint64_t)page > base){
        return page;
    }
    void* pg = GetPageAboveBase(base);
    FreePage(page);
    return pg;
    
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