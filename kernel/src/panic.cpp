#include <panic.h>
#include <kstdio.h>
#include <scheduling/apic/lapic.h>
#include <interrupts/interrupts.h>

void panic(const char* str, ...){
    va_list args;
    va_start(args, str);

    kprintf("\e[41;30mKERNEL PANIC!\e[0m\n");
    kprintfva(str, args);
    
    va_end(args);

    __asm__ ("cli");
    while(1) __asm__ ("pause");
}