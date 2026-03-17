
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

    .drivers : ALIGN(8) {
        __start_drivers = .;

        KEEP(*(SORT_BY_NAME(.drivers)))

        __stop_drivers = .;
    }

    .syscalls : ALIGN(8) {
        __start_syscalls = .;

        KEEP(*(SORT_BY_NAME(.syscalls)))

        __stop_syscalls = .;
    }

    .bss :
    {
        *(.bss .bss.*)
    }

    _KernelEnd = .;
}
