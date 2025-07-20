#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../paging/PageFrameAllocator.h"

void* dma_alloc(size_t size, size_t alignment = 0x1000, size_t boundary = 0);
void dma_free(void* address);


uint64_t dma_phys_addr(void* vaddr);