
#include <memory/heap.h>
#include <paging/PageTableManager.h>
#include <paging/PageFrameAllocator.h>
#include <BasicRenderer.h>
#include <scheduling/task/scheduler.h>
#include <scheduling/lock/spinlock.h>

void* heapStart = nullptr;
void* heapEnd = nullptr;
uint64_t HeapIntCnt = 0;
HeapSegHdr* LastHdr;

uint32_t heapSize = 0;
uint32_t heapUsed = 0;
uint32_t heapFree = 0;
uint8_t heap_magic[3] = {0xFA, 0xAA, 0xAC};

spinlock_t spinlock;


void InitializeHeap(void* heapAddr, size_t pageCount){
    void* pos = heapAddr;
    for(size_t i = 0; i < pageCount; i++){
        void* page = GlobalAllocator.RequestPage();
        if (page == nullptr){
            pageCount++;
            continue;
        }
        memset(page, 0, 0x1000);
        globalPTM.UnmapMemory(page); // remove the physical (1:1) mapping
        globalPTM.MapMemory(pos, page); //Map it to the heap address
        heapFree += 0x1000;
        heapSize += 0x1000;

        HeapIntCnt++;
        pos = (void*)((uint64_t)pos+0x1000);
    }

    size_t heapLength = pageCount * 0x1000;

    heapStart = heapAddr;
    heapEnd = (void*)((size_t)heapStart + heapLength);

    HeapSegHdr* startSeg = (HeapSegHdr*)heapAddr;
    startSeg->magic[0] = heap_magic[0];
    startSeg->magic[1] = heap_magic[1];
    startSeg->magic[2] = heap_magic[2];

    startSeg->length = heapLength - sizeof(HeapSegHdr);
    startSeg->Next = NULL;
    startSeg->Last = NULL;
    startSeg->free = true;
    LastHdr = startSeg;
}

extern "C" void free(void* address){
    if (address == nullptr){
        return;
    }
    HeapSegHdr* segment = (HeapSegHdr*)((uint8_t*)address - sizeof(HeapSegHdr));

    if ((void*)segment < heapStart || (void*)segment >= heapEnd){
        return;
    }
    if (segment->free){
        return;
    }
    if (memcmp(segment->magic, heap_magic, 3) == 0){
        return;
    }

    heapFree += segment->length;
    heapUsed -= segment->length;

    segment->free = true;
    segment->CombineForward();
    segment->CombineBackward();
    
    //memset((void*)((uint64_t)segment + sizeof(HeapSegHdr)), 0, segment->length - sizeof(HeapSegHdr));

}

extern "C" void* malloc(size_t size){

    size = (size + 0xF) & ~0xF;
    if (size == 0){
        return nullptr;
    }

    HeapSegHdr* currentSeg = (HeapSegHdr*) heapStart;
    currentSeg->magic[0] = heap_magic[0];
    currentSeg->magic[1] = heap_magic[1];
    currentSeg->magic[2] = heap_magic[2];

    if (currentSeg == nullptr){
        return nullptr;
    }

    while (true){
        if(currentSeg->free){
            if(currentSeg->length == size){
                currentSeg->free = false;
                heapFree -= currentSeg->length;
                heapUsed += currentSeg->length;
                return (void*)((uint64_t)currentSeg + sizeof(HeapSegHdr));
            }

            if(currentSeg->length > size){
                HeapSegHdr* newSeg = currentSeg->Split(size);
                currentSeg->free = false;
                heapFree -= currentSeg->length;
                heapUsed += currentSeg->length;
                return (void*)((uint64_t)currentSeg + sizeof(HeapSegHdr));
            }
        }
        if(currentSeg->Next == NULL) break;
        currentSeg = currentSeg->Next;
    }

    int sz = size - heapFree + 1024;
    ExpandHeap(sz > 0 ? sz : size);


    return malloc(size);
}

HeapSegHdr* HeapSegHdr::Split(size_t splitLength){
    if (splitLength < 0x10) return NULL;

    int64_t splitSegLength = length - splitLength - sizeof(HeapSegHdr);
    if (splitSegLength < 0x10) return NULL;

    HeapSegHdr* newSplitHdr = (HeapSegHdr*)((size_t)this + splitLength + sizeof(HeapSegHdr));
    newSplitHdr->magic[0] = heap_magic[0];
    newSplitHdr->magic[1] = heap_magic[1];
    newSplitHdr->magic[2] = heap_magic[2];

    newSplitHdr->free = true;
    newSplitHdr->length = splitSegLength;

    newSplitHdr->Next = this->Next;
    newSplitHdr->Last = this;

    if (this->Next != NULL)
        this->Next->Last = newSplitHdr;

    this->Next = newSplitHdr;
    this->length = splitLength;

    if (LastHdr == this)
        LastHdr = newSplitHdr;

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
        void* page = GlobalAllocator.RequestPage();
        globalPTM.UnmapMemory(page); 
        globalPTM.MapMemory(heapEnd, page);

        task_t* task = task_list;
        while(task != nullptr){
            if (task->ptm != nullptr){
                task->ptm->UnmapMemory(page); 
                task->ptm->MapMemory(heapEnd, page);
            }

            task = task->next;
        }
        /*for (int p = 0; p < taskScheduler::numOfTaskLists; p++){
            taskScheduler::tasklist_t* list = &taskScheduler::tasklists[p];
            
            for (int i = 0; i < list->numOfTasks; i++){
                taskScheduler::task_t* task = list->tasks[i];

                if (task->valid == false) continue;
                if (task->ptm == nullptr) continue;
                task->ptm->MapMemory(heapEnd, page);
            }
        }*/
        heapEnd = (void*)((size_t)heapEnd + 0x1000);
    }

    newSeg->magic[0] = heap_magic[0];
    newSeg->magic[1] = heap_magic[1];
    newSeg->magic[2] = heap_magic[2];

    newSeg->free = true;
    newSeg->Last = LastHdr;
    LastHdr->Next = newSeg;
    LastHdr = newSeg;
    newSeg->Next = NULL;
    newSeg->length = length - sizeof(HeapSegHdr);

    heapSize += length;

    newSeg->CombineBackward();
}
void HeapSegHdr::CombineForward(){
    if (Next == NULL || !Next->free) return;

    if (Next == LastHdr) LastHdr = this;

    if (Next->Next != NULL) Next->Next->Last = this;

    length = length + Next->length + sizeof(HeapSegHdr);
    Next = Next->Next;
}

void HeapSegHdr::CombineBackward(){
    if (Last != NULL && Last->free) Last->CombineForward();
}

extern "C" void* realloc(void* address, size_t sz){
    free(address);
    return malloc(sz);
}

