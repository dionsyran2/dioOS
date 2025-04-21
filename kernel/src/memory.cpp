
#include "memory.h"
#include <stdint.h>
#include <stddef.h>

uint64_t GetMemorySize(EFI_MEMORY_DESCRIPTOR* mMap, uint64_t mMapEntries, uint64_t mMapDescSize){

    static uint64_t memorySizeBytes = 0;
    if (memorySizeBytes > 0) return memorySizeBytes;

    for (int i = 0; i < mMapEntries; i++){
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)mMap + (i * mMapDescSize));
        memorySizeBytes += desc->numPages * 4096;
    }

    return memorySizeBytes;

}
void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

void* memset(void* start, int value, uint64_t num){
    for (uint64_t i = 0; i < num; i += sizeof(uint32_t)){
        *(uint64_t*)((uint64_t)start + i) = value;
    }
    return start;
}

void* memcpy(void *dest, const void *src, uint64_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    // Align dest and src to 8 bytes boundary for 64-bit copying
    while (((uintptr_t)d % 8 != 0) && n > 0) {
        *d++ = *s++;
        n--;
    }

    // Copy data in 64-bit chunks if possible
    uint64_t *d64 = (uint64_t *)d;
    const uint64_t *s64 = (const uint64_t *)s;
    while (n >= 8) {
        *d64++ = *s64++;
        n -= 8;
    }

    // Copy remaining bytes (less than 8 bytes)
    d = (uint8_t *)d64;
    s = (const uint8_t *)s64;
    while (n > 0) {
        *d++ = *s++;
        n--;
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}