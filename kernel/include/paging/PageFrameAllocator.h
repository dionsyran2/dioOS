#pragma once
#include <stdint.h>
#include <Bitmap.h>
#include <memory.h>
#include <limine.h>

//remove
#include <cstr.h>
#include <drivers/serial.h>

class PageFrameAllocator {
    public:
    void ReadEFIMemoryMap(limine_memmap_response* mmap);
    Bitmap PageBitmap;
    void FreePage(void* address);
    void FreePages(void* address, uint64_t pageCount);
    void LockPage(void* address);
    void LockPages(void* address, uint64_t pageCount);
    void* RequestPages(uint32_t pages);
    void* RequestPage();
    void* GetPageAboveBase(uint64_t);
    uint64_t GetFreeRAM();
    uint64_t GetUsedRAM();
    uint64_t GetReservedRAM();
    uint64_t GetMemSize();


    private:
    void InitBitmap(size_t bitmapSize, void* bufferAddress);
    void ReservePage(void* address);
    void ReservePages(void* address, uint64_t pageCount);
    void UnreservePage(void* address);
    void UnreservePages(void* address, uint64_t pageCount);

};
extern uint64_t freeMemory;
extern uint64_t reservedMemory;
extern uint64_t usedMemory;
extern PageFrameAllocator GlobalAllocator;
extern uint64_t lowestFreeSeg;
extern size_t lowestFreeSegSize; 