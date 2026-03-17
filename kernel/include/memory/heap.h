/* heap.h */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/spinlock/spinlock.h>


struct heap_header{
    char magic[4];

    heap_header* previous;
    heap_header* next;
    size_t size;
    bool free;

    char allignment[7];

    void merge_forwards();
    heap_header* merge_backwards();
    void split(size_t size);
} __attribute__((aligned(16)));


void InitializeHeap(uint64_t base_address, size_t size);

void* malloc(size_t size);
void free(void* memory);

extern uint64_t heap_used;
extern uint64_t heap_size;


inline void* operator new(size_t size){return malloc(size);};
inline void* operator new[](size_t size){return malloc(size);};

inline void operator delete(void* p){free(p);};
inline void operator delete(void* p, long unsigned int){free(p);};
inline void operator delete [](void* p, long unsigned int){free(p);}
inline void operator delete[](void* p){free(p);}