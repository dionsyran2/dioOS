#include <paging/PageFrameAllocator.h>
#include <scheduling/multiprocessor/ap_init.h>
#include <memory.h>

/* Track the lowest available segment */
/* So we can place the application processor*/
/* trampoline code there */

uint64_t ap_trampoline_base = UINT64_MAX;
uint64_t ap_trampoline_region_size;

// GlobalAllocator definition
PageFrameAllocator GlobalAllocator;

#include <drivers/serial/serial.h>

// @brief Initializes the Allocator
bool PageFrameAllocator::Initialize(limine_memmap_response* memmap){
    spinlock = 0;
    page_table = nullptr;
    total_memory = 0;
    
    uint64_t highest_address = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++){
        limine_memmap_entry* entry = memmap->entries[i];
        total_memory += entry->length;
        if (entry->base + entry->length > highest_address) highest_address = entry->base + entry->length;
    }

    free_memory = 0;
    reserved_memory = 0;
    used_memory = 0;
    
    total_pages = highest_address / 0x1000;
    uint64_t memory_needed = sizeof(page_struct) * total_pages;

    // --- Find memory for the page table ---
    uint64_t table_mem = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++){
        limine_memmap_entry* entry = memmap->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE) continue;
        if (entry->length >= memory_needed){
            table_mem = entry->base;
            break;
        }
    }
    if (table_mem == 0) return false;

    page_table = (page_struct*)physical_to_virtual(table_mem);
    
    // Clear the memory
    memset(page_table, 0, memory_needed);

    for (uint64_t i = 0; i < total_pages; i++){
        page_table[i].flags |= PAGE_FLAGS_LOCKED;
    }

    reserved_memory = total_memory; // Roughly

    size_t blob_size = (size_t)(trampoline_blob_end - trampoline_blob_start);
    
    for (uint64_t i = 0; i < memmap->entry_count; i++){
        limine_memmap_entry* entry = memmap->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE) continue;

        // Trampoline logic (kept same)
        if (entry->base < ap_trampoline_base && entry->length > blob_size){
            ap_trampoline_base = entry->base;
            ap_trampoline_region_size = entry->length;
        }

        uint64_t start_frame = entry->base / 0x1000;
        uint64_t page_count = entry->length / 0x1000;

        // Bounds check to be safe
        if (start_frame + page_count > total_pages) page_count = total_pages - start_frame;

        for (uint64_t j = 0; j < page_count; j++){
             page_table[start_frame + j].flags &= ~PAGE_FLAGS_LOCKED; 
        }

        // Update stats manually
        reserved_memory -= entry->length;
        free_memory += entry->length;
    }

    ReservePages(0, DIV_ROUND_UP(1 * 1024 * 1024, PAGE_SIZE)); 
    ReservePages(page_table, DIV_ROUND_UP(memory_needed, PAGE_SIZE));
    ReservePages((void*)ap_trampoline_base, DIV_ROUND_UP(blob_size, PAGE_SIZE));

    return true;
}

bool PageFrameAllocator::LockPage(void* page){
    // Get the physical address
    uint64_t physical = (uint64_t)page >= MEMORY_BASE ? virtual_to_physical((uint64_t)page) : (uint64_t)page;

    // Calculate the index
    uint64_t page_index = physical / 0x1000;
    bool out_of_bounds = page_index > total_pages;

    // bounds checking
    if (out_of_bounds) return false;
    
    // Get the actual entry
    page_struct* entry = &page_table[page_index];

    // Acquire a spinlock
    //uint64_t rflags = spin_lock(&entry->spinlock);
    
    // Set the LOCKED bit
    entry->flags |= PAGE_FLAGS_LOCKED;

    // Free the spinlock
    //spin_unlock(&entry->spinlock, rflags);

    // return
    return true;
}

bool PageFrameAllocator::LockPages(void* page, size_t amount){
    uint64_t physical = (uint64_t)page >= MEMORY_BASE ? virtual_to_physical((uint64_t)page) : (uint64_t)page;
    uint64_t start_index = physical / 0x1000;
    
    if (start_index + amount > total_pages) return false;

    for (size_t i = 0; i < amount; i++){
        page_table[start_index + i].flags |= PAGE_FLAGS_LOCKED;
    }
    return true;
}


bool PageFrameAllocator::UnlockPage(void* page){
    // Get the physical address
    uint64_t physical = (uint64_t)page >= MEMORY_BASE ? virtual_to_physical((uint64_t)page) : (uint64_t)page;

    // Calculate the index
    uint64_t page_index = physical / 0x1000;
    bool out_of_bounds = page_index > total_pages;

    // bounds checking
    if (out_of_bounds) return false;
    
    // Get the actual entry
    page_struct* entry = &page_table[page_index];

    // Acquire a spinlock
    //uint64_t rflags = spin_lock(&entry->spinlock);

    // Clear the LOCKED bit
    entry->flags &= ~PAGE_FLAGS_LOCKED;
    
    // Free the spinlock
    //spin_unlock(&entry->spinlock, rflags);

    // return
    return true;
}


bool PageFrameAllocator::UnlockPages(void* page, size_t amount){
    uint64_t physical = (uint64_t)page >= MEMORY_BASE ? virtual_to_physical((uint64_t)page) : (uint64_t)page;
    uint64_t start_index = physical / 0x1000;
    
    if (start_index + amount > total_pages) return false;

    for (size_t i = 0; i < amount; i++){
        page_table[start_index + i].flags &= ~PAGE_FLAGS_LOCKED;
    }
    return true;
}


bool PageFrameAllocator::ReservePage(void* page){
    uint64_t rflags = spin_lock(&spinlock); // Acquire the PFA spinlock
    reserved_memory += 0x1000;
    free_memory -= 0x1000;

    bool ret = LockPage(page);
    spin_unlock(&spinlock, rflags); // Free the spinlock
    return ret;
}

bool PageFrameAllocator::ReservePages(void* page, size_t amount){
    uint64_t rflags = spin_lock(&spinlock); // Acquire the PFA spinlock

    reserved_memory += (amount * 0x1000);
    free_memory -= (amount * 0x1000);
    
    bool ret = LockPages(page, amount);

    spin_unlock(&spinlock, rflags); // Free the spinlock
    return ret;
}

bool PageFrameAllocator::UnreservePage(void* page){
    reserved_memory -= 0x1000;
    free_memory += 0x1000;

    if (!UnlockPage(page)) return false;
    return true;
}

bool PageFrameAllocator::UnreservePages(void* page, size_t amount){
    reserved_memory -= (amount * 0x1000);
    free_memory += (amount * 0x1000);

    if (!UnlockPages(page, amount)) return false;
    return true;
}

bool PageFrameAllocator::IsLocked(uint64_t page){
    // Get the physical address
    uint64_t physical = (uint64_t)page >= MEMORY_BASE ? virtual_to_physical((uint64_t)page) : (uint64_t)page;

    // Calculate the index
    uint64_t page_index = physical / 0x1000;
    bool out_of_bounds = page_index > total_pages;

    // bounds checking
    if (out_of_bounds) return false;
    
    // Get the actual entry
    page_struct* entry = &page_table[page_index];

    // Acquire a spinlock
    //uint64_t rflags = spin_lock(&entry->spinlock);

    bool is_locked = entry->flags & PAGE_FLAGS_LOCKED;
    
    // Free the spinlock
    //spin_unlock(&entry->spinlock, rflags);

    return is_locked;
}

int PageFrameAllocator::GetReferenceCount(uint64_t page){
    // Get the physical address
    uint64_t physical = (uint64_t)page >= MEMORY_BASE ? virtual_to_physical((uint64_t)page) : (uint64_t)page;

    // Calculate the index
    uint64_t page_index = physical / 0x1000;
    bool out_of_bounds = page_index > total_pages;

    // bounds checking
    if (out_of_bounds) return 0;
    
    // Get the actual entry
    page_struct* entry = &page_table[page_index];

    // Acquire a spinlock
    uint64_t rflags = spin_lock(&entry->spinlock);

    int count = entry->reference_count;
    
    // Free the spinlock
    spin_unlock(&entry->spinlock, rflags);

    return count;
}

bool PageFrameAllocator::SetReferenceCount(uint64_t page, int ref_cnt){
    // Get the physical address
    uint64_t physical = (uint64_t)page >= MEMORY_BASE ? virtual_to_physical((uint64_t)page) : (uint64_t)page;

    // Calculate the index
    uint64_t page_index = physical / 0x1000;
    bool out_of_bounds = page_index > total_pages;

    // bounds checking
    if (out_of_bounds) return false;
    
    // Get the actual entry
    page_struct* entry = &page_table[page_index];

    // Acquire a spinlock
    uint64_t rflags = spin_lock(&entry->spinlock);

    entry->reference_count = ref_cnt;
    
    // Free the spinlock
    spin_unlock(&entry->spinlock, rflags);
    return true;
}

void* PageFrameAllocator::RequestPage(){
    return RequestPages(1);
}

void* PageFrameAllocator::RequestPages(size_t amount){
    if (amount == 0) return nullptr;
    uint64_t rflags = spin_lock(&spinlock);

    uint64_t start_idx = last_free_index;
    size_t free_count = 0;

    for (uint64_t i = last_free_index; i < total_pages; i++){
        if (page_table[i].flags & PAGE_FLAGS_LOCKED) {
            free_count = 0;
            start_idx = i + 1;
            continue;
        }

        free_count++;

        if (free_count == amount) {
            for (size_t j = 0; j < amount; j++){
                page_table[start_idx + j].flags |= PAGE_FLAGS_LOCKED;
                page_table[start_idx + j].reference_count = 1;
            }

            free_memory -= (amount * 0x1000);
            used_memory += (amount * 0x1000);
            last_free_index = start_idx + amount; // Don't check these again next time

            spin_unlock(&spinlock, rflags);
            return (void*)physical_to_virtual(start_idx * 0x1000);
        }
    }

    spin_unlock(&spinlock, rflags);
    return nullptr; // Out of memory!
}

void PageFrameAllocator::FreePage(void* page){
    return FreePages(page, 1);
}

void PageFrameAllocator::FreePages(void* page, size_t amount){
    uint64_t physical = (uint64_t)page >= MEMORY_BASE ? virtual_to_physical((uint64_t)page) : (uint64_t)page;
    
    if (physical < 1048576) return; 

    uint64_t start_index = physical / 0x1000;

    uint64_t rflags = spin_lock(&spinlock);

    if (start_index < last_free_index) last_free_index = start_index;
    free_memory += (amount * 0x1000);
    used_memory -= (amount * 0x1000);

    // Unlock them all
    for (uint64_t i = 0; i < amount; i++){
        page_table[start_index + i].flags &= ~PAGE_FLAGS_LOCKED;
    }

    spin_unlock(&spinlock, rflags);
}

void PageFrameAllocator::IncreaseReferenceCount(void* page){
    // Get the physical address
    uint64_t physical = (uint64_t)page >= MEMORY_BASE ? virtual_to_physical((uint64_t)page) : (uint64_t)page;

    // Calculate the index
    uint64_t page_index = physical / 0x1000;
    bool out_of_bounds = page_index > total_pages;

    // bounds checking
    if (out_of_bounds) return;
    
    // Get the actual entry
    page_struct* entry = &page_table[page_index];

    // Acquire a spinlock
    uint64_t rflags = spin_lock(&entry->spinlock);
    
    entry->reference_count++; 

    // Free the spinlock
    spin_unlock(&entry->spinlock, rflags);
}


void PageFrameAllocator::DecreaseReferenceCount(void* page){
    // Get the physical address
    uint64_t physical = (uint64_t)page >= MEMORY_BASE ? virtual_to_physical((uint64_t)page) : (uint64_t)page;

    // Calculate the index
    uint64_t page_index = physical / 0x1000;
    bool out_of_bounds = page_index > total_pages;

    // bounds checking
    if (out_of_bounds) return;
    
    // Get the actual entry
    page_struct* entry = &page_table[page_index];

    // Acquire a spinlock
    uint64_t rflags = spin_lock(&entry->spinlock);
    
    if (entry->reference_count > 0) entry->reference_count--;

    bool should_free = false;
    if (entry->reference_count == 0) should_free = true;
    // Free the spinlock
    spin_unlock(&entry->spinlock, rflags);

    if (should_free) FreePage(page);
}
