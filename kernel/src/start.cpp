#include <stdint.h>
#include <stddef.h>

extern "C" typedef void (*constructor_t)();
extern "C" constructor_t __init_array_start[];
extern "C" constructor_t __init_array_end[];

extern "C" void main();

extern "C" [[noreturn]] void _start() {
    
    // Call all global/static C++ constructors
    // This loops through .init_array and initializes things like your linked lists!
    size_t count = __init_array_end - __init_array_start;
    for (size_t i = 0; i < count; i++) {
        __init_array_start[i]();
    }

    main();

    // Catch the CPU if _main ever accidentally returns
    while (1) {
        #if defined(__x86_64__)
            asm volatile("cli; hlt");
        #elif defined(__arm__) || defined(__aarch64__)
            asm volatile("cpsid i; wfi");
        #endif
    }
}