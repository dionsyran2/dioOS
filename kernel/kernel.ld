OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    _KernelStart = .;  /* Symbol marking the start of the kernel */

    /* Entry point for CPU startup code */
    .text.cpu_start : {
        *(.text.cpu_start)  /* Include all .text.cpu_start sections from input files */
    }

    /* Entry point for interrupt handler code */
    .text.interrupt_handler : {
        *(.text.interrupt_handler)  /* Include all .text.interrupt_handler sections from input files */
    }

    /* Main .text section for the kernel */
    .text : ALIGN(0x1000) {
        *(.text)  /* Include all .text sections from input files */
    }

    /* Data section */
    .data : ALIGN(0x1000) {
        *(.data)  /* Include all .data sections from input files */
    }

    /* Read-only data section */
    .rodata : ALIGN(0x1000) {
        *(.rodata)  /* Include all .rodata sections from input files */
    }

    /* Uninitialized data section */
    .bss : ALIGN(0x1000) {
        *(COMMON)  /* Common symbols */
        *(.bss)  /* Include all .bss sections from input files */
    }

    _KernelEnd = .;  /* Symbol marking the end of the kernel */
}