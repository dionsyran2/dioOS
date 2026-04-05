/* Kernel Heap */
#include <memory/heap.h>
#include <paging/PageTableManager.h>
#include <paging/PageFrameAllocator.h>
#include <memory.h>

char heap_magic[] = {'H', 'E', 'A', 'P'};

uint64_t heap_used = 0;
uint64_t heap_size = 0;

heap_header* heap_start = nullptr;
heap_header* heap_end = nullptr;
uint64_t LastHeapAddress = 0;

spinlock_t heap_lock;


/* Merge Forwards */
void heap_header::merge_forwards(){
    if (memcmp(this->next->magic, heap_magic, sizeof(this->next->magic))) return; // Invalid fw header

    this->size += this->next->size + sizeof(heap_header); // Increase the size
    
    if (heap_end == this->next) heap_end = this;

    this->next = this->next->next; // Update the chain

    if (this->next) {
        this->next->previous = this;
    }
}

/* Merge Backwards */
heap_header* heap_header::merge_backwards(){
    heap_header* new_hdr = this->previous;
    if (memcmp(new_hdr->magic, heap_magic, sizeof(new_hdr->magic))) return this; // Invalid bw header



    new_hdr->size += this->size + sizeof(heap_header);

    if (heap_end == this) heap_end = new_hdr;

    new_hdr->next = this->next;

    if (new_hdr->next) {
        new_hdr->next->previous = new_hdr;
    }

    return new_hdr;
}

/* Split */
// @warning The lock must be acquired before calling this
void heap_header::split(size_t split_size){
    if (split_size >= this->size) return; // If this segment's size is less than the new size, return
    if (this->size - split_size <= sizeof(heap_header)) return; // If there is no space to create a new segment (split) return


    heap_header* new_segment = (heap_header*)((uint64_t)this + split_size + sizeof(heap_header)); // The new segment (At the end of the current one)
    memset(new_segment, 0, sizeof(heap_header));
    // Set the magic
    memcpy(new_segment->magic, heap_magic, sizeof(new_segment->magic));

    new_segment->size = this->size - (split_size + sizeof(heap_header)); // Its size (the memory left after the split - its header)
    new_segment->free = true;

    this->size = split_size; // Set the new size
    
    // Fix the chain
    new_segment->previous = this;
    new_segment->next = this->next;

    if (this->next && (uint64_t)this->next >= HEAP_BASE){
        this->next->previous = new_segment;
    }

    this->next = new_segment;


    if (heap_end == this) heap_end = new_segment;

    // We are done!
}

/* Initialize */
void InitializeHeap(uint64_t base_address, size_t size){
    uint64_t rflags = spin_lock(&heap_lock);

    LastHeapAddress = base_address;
    heap_start = (heap_header*)base_address;
    heap_end = (heap_header*)base_address;

    heap_used = 0;
    heap_size = 0;

    for (size_t o = 0; o < size; o += 0x1000){
        void* page = GlobalAllocator.RequestPage();
        if (page == nullptr) break; 

        globalPTM.MapMemory((void*)LastHeapAddress, (void*)virtual_to_physical((uint64_t)page));

        // Only increment the addresses here. Don't touch the header size yet.
        LastHeapAddress += 0x1000;
        heap_size += 0x1000;
    }

    memset(heap_start, 0, heap_size);

    heap_start->size = heap_size - sizeof(heap_header); 
    heap_start->free = true;
    heap_start->previous = nullptr;
    heap_start->next = nullptr;
    
    memcpy(heap_start->magic, heap_magic, sizeof(heap_start->magic));

    spin_unlock(&heap_lock, rflags);
}

/* Expand Heap */
// @warning The lock must be acquired before calling this
void ExpandHeap(size_t size){

    size_t previous_size = heap_end->size;

    for (size_t o = 0; o < size; o += 0x1000){
        // Allocate a page
        void* page = GlobalAllocator.RequestPage();
        if (page == nullptr) break;
        memset(page, 0, PAGE_SIZE);

        // Map it
        globalPTM.MapMemory((void*)LastHeapAddress, (void*)virtual_to_physical((uint64_t)page));

        // Update the segment
        heap_end->size += 0x1000; // Update the segment header

        // Update the address
        LastHeapAddress += 0x1000;

        heap_size += 0x1000;
    }

    if (!heap_end->free) heap_end->split(previous_size);

}

#include <panic.h>
void fix_corrupted_stack(){
    panic("Heap corruption detected!\n");
    
    /*for (heap_header *segment = heap_start; segment != nullptr; segment = segment->next){
        if ((uint64_t)segment->next < HEAP_BASE && segment->next != nullptr) {
            segment->next = nullptr;
            heap_end = segment;
            break;
        }
    }*/
}

heap_header* find_free_segment(size_t size){
    heap_header* segment = heap_start;

    while (segment != nullptr) {
        if ((uint64_t)segment->next < HEAP_BASE && segment->next != nullptr) {
            fix_corrupted_stack();
            return find_free_segment(size);    
        }
        if (segment->free && segment->size >= size) {
            // Split if we have enough excess space
            if (segment->size > size + sizeof(heap_header)) {
                segment->split(size);
            }

            return segment;
        }

        segment = segment->next;

    }

    return nullptr;
}

void* malloc(size_t size){
    if (size == 0) return nullptr;
    
    size = (size + 0xF) & ~((size_t)0xF);

    uint64_t rflags = spin_lock(&heap_lock);

    heap_header* segment = find_free_segment(size);

    if (segment == nullptr){
        ExpandHeap(size * 2);
        segment = find_free_segment(size);
    }

    if (segment == nullptr) {
        spin_unlock(&heap_lock, rflags);

        return nullptr;
    }

    segment->free = false;

    heap_used += segment->size;
    spin_unlock(&heap_lock, rflags);

    return (void*)((uint64_t)segment + sizeof(heap_header));
}

#include <kstdio.h>
void free(void* memory){
    if (memory == nullptr) return;

    heap_header* segment = (heap_header*)((uint64_t)memory - sizeof(heap_header));

    // Sanity Checks
    if (memcmp(segment->magic, heap_magic, sizeof(segment->magic)) != 0) return;
    uint64_t rflags = spin_lock(&heap_lock);

    if (segment->free) {
        int *r = (int*)0xDEADBEEF;
        *r = 0;
        serialf("\e[0;31m DOUBLE FREE!\e[0m\n");
        
        // Optional: Report double free error
        spin_unlock(&heap_lock, rflags);
        return;
    }

    // Don't want to use 158% of our heap memory (Update the heap_used value when freeing memory)
    heap_used -= segment->size;

    segment->free = true;
    
    // Merge Forwards
    if (segment->next && segment->next->free) 
        segment->merge_forwards();

    // Merge Backwards
    if (segment->previous && (uint64_t)segment->previous >= HEAP_BASE && segment->previous->free) 
        segment = segment->merge_backwards();

    spin_unlock(&heap_lock, rflags);
}