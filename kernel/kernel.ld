
OUTPUT_FORMAT(elf64-x86-64)

ENTRY(_start)

SECTIONS
{   
    . = 1M;
    _KernelStart = .;
    .multiboot_header : {
        *(.multiboot_header)
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