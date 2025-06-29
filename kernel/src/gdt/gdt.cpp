#include <gdt/gdt.h>

GDT DefaultGDT = {
    {0, 0, 0, 0x00, 0x00, 0}, // Null segment
    {0xFFFF, 0, 0, 0x9a, 0xAF, 0}, // Kernel code segment (DPL 0)
    {0xFFFF, 0, 0, 0x92, 0xCF, 0}, // Kernel data segment (DPL 0)

    // DID I MENTION HOW MUCH I HATE THOSE SYSRET SCHENANIGANS
    {0xFFFF, 0, 0, 0xF2, 0xCF, 0},        // User data segment (DPL 3)
    {0xFFFF, 0, 0, 0xFA, 0xAF, 0},        // User code segment (DPL 3)
    {0, 0, 0, 0, 0, 0, 0, 0, 0}, //Set programmatically
};