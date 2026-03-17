#include <memory.h>
#include <stdint.h>
#include <stddef.h>
#include <cpu.h>
#include <kimmintrin.h>


uint64_t MEMORY_BASE;

uint64_t physical_to_virtual(uint64_t addr){
	return addr + MEMORY_BASE;
}

uint64_t virtual_to_physical(uint64_t addr){
	return (addr >= MEMORY_BASE) ? addr - MEMORY_BASE : addr;
}

uint64_t get_virtual_device_mmio_address(uint64_t physical){
	return (MMIO_BASE + physical); // 1GB above the rest of the memory to avoid conflicts
}

uint64_t GetMemorySize(limine_memmap_response* mmap){
	uint64_t size = 0;
	for (int i = 0; i < mmap->entry_count; i++){
		size += mmap->entries[i]->length;
	}
	return size;
}


extern "C" int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

static inline bool is_aligned(const void* ptr, size_t alignment) {
    return ((uintptr_t)ptr % alignment) == 0;
}

// ============================================================================
// 1. Scalar (Fallback) - No special attributes needed
// ============================================================================
static void* memcpy_scalar(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    while (n >= 8) {
        *(uint64_t*)d = *(const uint64_t*)s;
        d += 8; s += 8; n -= 8;
    }
    while (n--) *d++ = *s++;
    return dest;
}

static void* memmove_scalar(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    if (d == s) return dest;

    if (d < s) {
        return memcpy_scalar(dest, src, n);
    } else {
        d += n; s += n;
        while (n >= 8) {
            d -= 8; s -= 8;
            *(uint64_t*)d = *(const uint64_t*)s;
            n -= 8;
        }
        while (n--) *(--d) = *(--s);
    }
    return dest;
}

void* memset_scalar(void* start, int value, uint64_t num){
    for (uint64_t i = 0; i < num;){
        if (num - i >= 4) {
			*(uint32_t*)((uint64_t)start + i) = value;
			i += sizeof(uint32_t);
		} else {
			*(uint8_t*)((uint64_t)start + i) = (value & (0xFFL << (i % sizeof(uint32_t))));
			i += sizeof(uint8_t);
		}
    }
    return start;
}

// ============================================================================
// 2. SSE Implementation - Target: SSE2
// ============================================================================
__attribute__((target("sse2")))
static void* memcpy_sse(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    while (n >= 16) {
        _mm_storeu_si128((__m128i*)d, _mm_loadu_si128((const __m128i*)s));
        d += 16; s += 16; n -= 16;
    }
    // Cleanup using scalar logic (safe within SSE function)
    while (n--) *d++ = *s++;
    return dest;
}

__attribute__((target("sse2")))
static void* memmove_sse(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    if (d == s) return dest;

    if (d < s) {
        return memcpy_sse(dest, src, n);
    } else {
        d += n; s += n;
        while (n >= 16) {
            d -= 16; s -= 16;
            _mm_storeu_si128((__m128i*)d, _mm_loadu_si128((const __m128i*)s));
            n -= 16;
        }
        while (n--) *(--d) = *(--s);
    }
    return dest;
}

__attribute__((target("sse2")))
static void* memset_sse(void* start, int value, size_t num) {
    uint8_t* p = (uint8_t*)start;
    
    // Broadcast value into 128-bit register
    __m128i vec = _mm_set1_epi8((char)value);

    // 16-byte chunks
    while (num >= 16) {
        _mm_storeu_si128((__m128i*)p, vec);
        p += 16;
        num -= 16;
    }
    
    // Cleanup scalar
    while (num--) *p++ = (uint8_t)value;
    return start;
}

// ============================================================================
// 3. AVX Implementation - Target: AVX
// ============================================================================
// We use "avx" because _mm256_loadu_si256 is part of AVX1. 
__attribute__((target("avx")))
static void* memset_avx(void* start, int value, size_t num) {
    uint8_t* p = (uint8_t*)start;

    // 1. Prepare 128-bit part
    __m128i vec128 = _mm_set1_epi8((char)value);
    __m128 vec128_f = _mm_castsi128_ps(vec128);
    
    // 2. Double it to 256-bit using insertf128 (AVX1 specific)
    __m256 vec256 = _mm256_castps128_ps256(vec128_f);
    vec256 = _mm256_insertf128_ps(vec256, vec128_f, 1);

    while (num >= 32) {
        _mm256_storeu_ps((float*)p, vec256);
        p += 32; num -= 32;
    }
    while (num--) *p++ = (uint8_t)value;
    return start;
}

__attribute__((target("avx")))
static void* memcpy_avx(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    while (n >= 32) {
        _mm256_storeu_si256((__m256i*)d, _mm256_loadu_si256((const __m256i*)s));
        d += 32; s += 32; n -= 32;
    }
    while (n--) *d++ = *s++;
    return dest;
}

__attribute__((target("avx")))
static void* memmove_avx(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    if (d == s) return dest;

    if (d < s) {
        return memcpy_avx(dest, src, n);
    } else {
        d += n; s += n;
        while (n >= 32) {
            d -= 32; s -= 32;
            _mm256_storeu_si256((__m256i*)d, _mm256_loadu_si256((const __m256i*)s));
            n -= 32;
        }
        while (n--) *(--d) = *(--s);
    }
    return dest;
}


// ============================================================================
// 3b. AVX2 Implementation (Target: AVX2)
// Safe for Haswell and newer. Uses proper Integer broadcast.
// ============================================================================
__attribute__((target("avx2")))
static void* memset_avx2(void* start, int value, size_t num) {
    uint8_t* p = (uint8_t*)start;

    // AVX2 allows broadcasting a single byte directly to 256 bits
    __m256i vec = _mm256_set1_epi8((char)value);

    while (num >= 32) {
        // Use Integer Store (AVX2 specific)
        _mm256_storeu_si256((__m256i*)p, vec);
        p += 32; num -= 32;
    }
    while (num--) *p++ = (uint8_t)value;
    return start;
}

// ============================================================================
// 4. AVX-512 Implementation - Target: AVX512F
// ============================================================================
__attribute__((target("avx512f")))
static void* memcpy_avx512(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    while (n >= 64) {
        _mm512_storeu_si512(d, _mm512_loadu_si512(s));
        d += 64; s += 64; n -= 64;
    }
    while (n--) *d++ = *s++;
    return dest;
}

__attribute__((target("avx512f")))
static void* memmove_avx512(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    if (d == s) return dest;

    if (d < s) {
        return memcpy_avx512(dest, src, n);
    } else {
        d += n; s += n;
        while (n >= 64) {
            d -= 64; s -= 64;
            _mm512_storeu_si512(d, _mm512_loadu_si512(s));
            n -= 64;
        }
        while (n--) *(--d) = *(--s);
    }
    return dest;
}

__attribute__((target("avx512f")))
static void* memset_avx512(void* start, int value, size_t num) {
    uint8_t* p = (uint8_t*)start;

    // AVX512 makes this easy again
    __m512i vec = _mm512_set1_epi8((char)value);

    while (num >= 64) {
        _mm512_storeu_si512(p, vec);
        p += 64;
        num -= 64;
    }

    while (num--) *p++ = (uint8_t)value;
    return start;
}

// ============================================================================
// Dispatchers
// ============================================================================

extern "C" void* memcpy(void* dest, const void* src, size_t n) {
    if (is_avx512_enabled) {
        return memcpy_avx512(dest, src, n);
    }
    if (is_avx_enabled) {
        return memcpy_avx(dest, src, n);
    }
    if (is_sse_enabled) {
        return memcpy_sse(dest, src, n);
    }
    return memcpy_scalar(dest, src, n);
}

extern "C" void* memmove(void* dest, const void* src, size_t n) {
    // Its so small that its probably worth it to use the scalar version
	if (n < 64) return memmove_scalar(dest, src, n);
    
    if (is_avx512_enabled) {
        return memmove_avx512(dest, src, n);
    }
    if (is_avx_enabled) {
        return memmove_avx(dest, src, n);
    }
    if (is_sse_enabled) {
        return memmove_sse(dest, src, n);
    }
    return memmove_scalar(dest, src, n);
}

extern "C" void* memset(void* start, int value, size_t num) {
    // 1. AVX-512 (King of bandwidth)
    if (is_avx512_enabled) {
        return memset_avx512(start, value, num);
    }
    
    // 2. AVX2 (Modern standard, uses clean Integer instructions)
    if (is_avx2_enabled) {
        return memset_avx2(start, value, num);
    }
    
    // 3. AVX1 (Fallback for 2011-2013 CPUs, uses Float hack)
    if (is_avx_enabled) {
        return memset_avx(start, value, num);
    }
    
    // 4. SSE (Old standard)
    if (is_sse_enabled) {
        return memset_sse(start, value, num);
    }
    
    // 5. Scalar (Safe fallback)
    return memset_scalar(start, value, num);
}


// Memory Usage
#include <kstdio.h>
#include <paging/PageFrameAllocator.h>
#include <memory/heap.h>
void log_memory_usage(){
    uint64_t used_memory = GlobalAllocator.total_memory - GlobalAllocator.free_memory;
    uint64_t used_memory_percentage = (used_memory * 100) / GlobalAllocator.total_memory;
    kprintf("-------- MEMORY --------\n");
    kprintf("Used Memory: %d [%d Reserved] MB (%d%%)\n", used_memory / (1024 * 1024), GlobalAllocator.reserved_memory / (1024 * 1024), used_memory_percentage);
    kprintf("Total Memory: %d MB\n", GlobalAllocator.total_memory / (1024 * 1024));
    kprintf("-----------------------\n");

    uint64_t kheap_used_memory = heap_used;
    uint64_t kheap_used_percentage = (heap_used * 100) / heap_size; 
    kprintf("-------- KHEAP --------\n");
    kprintf("Used Memory %d KB (%d%%)\n", kheap_used_memory / (1024), kheap_used_percentage);
    kprintf("Total Memory: %d KB\n", heap_size / (1024));
    kprintf("-----------------------\n");
}