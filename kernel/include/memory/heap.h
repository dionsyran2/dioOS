#pragma once

#include <stdint.h>
#include <stddef.h>

struct HeapSegHdr
{   
    uint8_t magic[3];
    size_t length;
    HeapSegHdr* Next;
    HeapSegHdr* Last;
    bool free;
    void CombineForward();
    void CombineBackward();
    HeapSegHdr* Split(size_t splitLength);
};


void InitializeHeap(void* HeapAddress, size_t HeapLength);

extern "C" void* malloc(size_t size);
extern "C" void free(void* address);
extern "C" void* realloc(void* address, size_t sz);
void ExpandHeap(size_t length);

inline void* operator new(size_t size){return malloc(size);};
inline void* operator new[](size_t size){return malloc(size);};

inline void operator delete(void* p){free(p);};
inline void operator delete(void* p, long unsigned int){free(p);};
inline void operator delete [](void* p, long unsigned int){free(p);}
inline void operator delete[](void* p){free(p);}

extern uint64_t HeapIntCnt;
extern uint32_t heapSize;
extern uint32_t heapUsed;
extern uint32_t heapFree;