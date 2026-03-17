#pragma once

#include <stdint.h>
#include <stdint.h>
#include <stddef.h>
#include <limine.h>

// @brief Rounds upwards
#define ROUND_UP(x, y)  ((((x) + ((y) - 1)) / (y)) * (y))

// @brief Rounds upwards, then devides by align
#define DIV_ROUND_UP(x, align) (((x) + (align) - 1) / (align))

// @brief Will align to y boundrary
#define ALIGN(x, y) (((x) + (y - 1)) & ~(y - 1))   

extern uint64_t MEMORY_BASE;
#define MMIO_BASE (MEMORY_BASE + 0x200000000000)
#define HEAP_BASE (MEMORY_BASE + 0x100000000000)

uint64_t get_virtual_device_mmio_address(uint64_t physical);
uint64_t physical_to_virtual(uint64_t addr);
uint64_t virtual_to_physical(uint64_t addr);


uint64_t GetMemorySize(limine_memmap_response* mmap);

extern "C" void* memset(void* start, int value, uint64_t num);
extern "C" int memcmp(const void *s1, const void *s2, size_t n);
extern "C" void *memmove(void *dest, const void *src, size_t n);
extern "C" void *memcpy(void* __restrict__ dest, const void* __restrict__ src, size_t n);

void log_memory_usage();
