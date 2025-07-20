#pragma once

#include <stdint.h>
#include "efiMemory.h"
#include <stdint.h>
#include <stddef.h>
#define DIV_ROUND_UP(x, align) (((x) + (align) - 1) / (align))

uint64_t GetMemorySize(EFI_MEMORY_DESCRIPTOR* mMap, uint64_t mMapEntries, uint64_t mMapDescSize);
void* memset(void* start, int value, uint64_t num);
void* memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memcpy_simd(void* __restrict__ dest, const void* __restrict__ src, size_t n);

