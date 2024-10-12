#include "heap.h"
#include "../paging/PageTableManager.h"
#include "../paging/PageFrameAllocator.h"

void* heapStart;
void* heapEnd;
HeapSegHdr* LastHdr;
void InitializeHeap(void* heapAddr, size_t pageCount){
    void* pos = heapAddr;

    for(size_t i = 0; i < pageCount; i++){
        globalPTM.MapMemory(pos, GlobalAllocator.RequestPage());
        pos = (void*)((size_t)pos+0x1000);
    }

    size_t heapLength = pageCount * 0x1000;

    heapStart = heapAddr;
    heapEnd = (void*)((size_t)heapStart + heapLength);
    HeapSegHdr* startSeg = (HeapSegHdr*)heapAddr;
    startSeg->length = heapLength - sizeof(HeapSegHdr);
    startSeg->Next = NULL;
    startSeg->Last = NULL;
    startSeg->free = true;
    LastHdr = startSeg;


}

void free(void* address){
    HeapSegHdr* segment = (HeapSegHdr*)address -1;
    segment->free = true;
    segment->CombineForward();
    segment->CombineBackward();
}

void* malloc(size_t size){
    if (size % 0x10 > 0){//not multiple of 0x10
        size -=(size % 0x10);
        size += 0x10;
    }

    if (size == 0) return NULL;

    HeapSegHdr* currentSeg = (HeapSegHdr*) heapStart;
    while (true)
    {
        if(currentSeg->free){
            if(currentSeg->length > size){
                currentSeg->Split(size);
                currentSeg->free = false;
                return (void*)((uint64_t)currentSeg + sizeof(HeapSegHdr));
            }
            if(currentSeg->length == size){
                currentSeg->free = false;
                return (void*)((uint64_t)currentSeg + sizeof(HeapSegHdr));
            }
        }
        if(currentSeg->Next == NULL){
            break;
        }
        currentSeg = currentSeg->Next;
    }
    ExpandHeap(size);
    return malloc(size);
}

HeapSegHdr* HeapSegHdr::Split(size_t splitLength){
    if (splitLength < 0x10) return NULL;
    int64_t splitSegLength = length - splitLength - (sizeof(HeapSegHdr));
    if (splitSegLength < 0x10) return NULL;
    HeapSegHdr* newSplitHdr = (HeapSegHdr*) ((size_t)this + splitLength + sizeof(HeapSegHdr));
    Next->Last = newSplitHdr;
    newSplitHdr->Next = Next;
    Next = newSplitHdr;
    newSplitHdr->Last = this;
    newSplitHdr->length = splitSegLength;
    newSplitHdr->free = free;
    length = splitLength;

    if (LastHdr == this) LastHdr = newSplitHdr;
    return newSplitHdr;
}

void ExpandHeap(size_t length){
    if (length % 0x1000 > 0){//not multiple of 0x10
        length -=(length % 0x1000);
        length += 0x1000;
    }

    size_t pageCount = length / 0x1000;
    HeapSegHdr* newSeg = (HeapSegHdr*)heapEnd;
    for(size_t i = 0; i < pageCount; i++){
        globalPTM.MapMemory(heapEnd, GlobalAllocator.RequestPage());
        heapEnd = (void*)((size_t)heapEnd + 0x1000);
    }

    newSeg->free = true;
    newSeg->Last = LastHdr;
    LastHdr->Next = newSeg;
    LastHdr = newSeg;
    newSeg->Next = NULL;
    newSeg->length = length - sizeof(HeapSegHdr);
    newSeg->CombineBackward();
}
void HeapSegHdr::CombineForward(){
    if (Next == NULL) return;
    if (!Next->free) return;

    if(Next == LastHdr) LastHdr = this;

    if (Next->Next != NULL) Next->Next->Last = this;
    
    length = length + Next->length + sizeof(HeapSegHdr);
    Next= Next->Next;
}

void HeapSegHdr::CombineBackward(){
    if (Last != NULL && Last->free) Last->CombineForward();
}