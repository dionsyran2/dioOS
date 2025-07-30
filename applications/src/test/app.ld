OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x70000000000;
    
    _start = .;
    
    .text :
    {
        *(.text)
    }
    
    .rodata :
    {
        *(.rodata)
    }
    
    .data :
    {
        *(.data)
    }
    
    .bss (NOLOAD) :
    {
        *(COMMON)
        *(.bss)
    }
}
