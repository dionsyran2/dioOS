#pragma once

#include <stdint.h>
#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#define DIV_ROUND_UP(x, align) (((x) + (align) - 1) / (align))
#define ALIGN(x, y) (((x) + (y - 1)) & ~(y - 1))   

extern uint64_t MEMORY_BASE;

uint64_t physical_to_virtual(uint64_t addr);
uint64_t virtual_to_physical(uint64_t addr);


uint64_t GetMemorySize(limine_memmap_response* mmap);

void* memset(void* start, int value, uint64_t num);
void* memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memcpy_simd(void* __restrict__ dest, const void* __restrict__ src, size_t n);

