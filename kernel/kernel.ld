
OUTPUT_FORMAT(elf64-x86-64)

ENTRY(main)

SECTIONS
{   
    . = 0xFFFFFFFF80000000;
    _KernelStart = .;
    
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    }

    .text :
    {
        *(.text)
    }

    .rodata :
    {
        *(.rodata)
    }

    .rdata :
    {
        *(.rdata)
    }

    .data :
    {
        *(.data .data.*)
    }

    .bss :
    {
        *(.bss .bss.*)
    }

    _KernelEnd = .;
}
