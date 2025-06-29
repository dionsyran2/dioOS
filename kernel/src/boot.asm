[bits 32]
SECTION .multiboot_header
header_start:
    DD 0xe85250d6 ; magic
    DD 0 ; protected mode i386
    DD header_end - header_start ; header size
    DD 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start)) ; checksum

align 8
framebuffer_tag_start:
    dw 5                                              ; type
    dw 1                                              ; flags
    dd framebuffer_tag_end - framebuffer_tag_start    ; size
    dd 1920
    dd 1080
    dd 32

framebuffer_tag_end:

align 8
    ; end tag
    DW 0
    DW 0
    DD 8
header_end:

section .text
global _start
extern _main

;The main idea is to load a temp GDT and Paging Tables, which will be changed later with cpp code.
;It should be much simpler (for me) and easier to debug
;If you are reading this and saying wtf is this guy doing... this is my os, do as you want with yours.
setupTempPT:
    mov eax, page_table_l3
    or eax, 0b11
    mov [page_table_l4], eax

    mov eax, page_table_l2
    or eax, 0b11
    mov [page_table_l3], eax

    mov ecx, 0 ;cnt
.loop:

    mov eax, 0x200000 ; 2MB
    mul ecx
    or eax, 0b10000011
    mov [page_table_l2 + ecx * 8], eax

    inc ecx

    cmp ecx, 512
    jne .loop

    ret

GetInLongMode:
    ;jmp $
    call setupTempPT

    ;PT
    mov eax, page_table_l4
    mov cr3, eax
    ;jmp $

    ;PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    ;jmp $

    ;Long Mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    ;jmp $

    ;Enable Paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ;jmp $

    lgdt [gdt64.pointer]

    ;jmp $

	jmp gdt64.code_segment:long_mode_start

    hlt



    

_start:
    ;jmp $
    mov esp, stack_top
    ;jmp $

    push 0
    push ebx
    push 0 ;It is used for padding, since right now we are pushing 4 bytes but in long mode we are popping 8
    push eax

    jmp GetInLongMode

section .bss
global stack_top
align 16
stack_bottom:
resb 1024 * 1024 * 100; 1MB stack size
stack_top:

align 4096
page_table_l4:
	resb 4096
page_table_l3:
	resb 4096
page_table_l2:
	resb 4096

section .rodata
gdt64:
	dq 0
.code_segment: equ $ - gdt64
	dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
.pointer:
	dw $ - gdt64 - 1
	dq gdt64

[bits 64]
section .text

extern _enable_avx


long_mode_start:
    ;jmp $
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ;jmp $

    call _enable_avx
    
    pop rdi
    pop rsi
    ;jmp $
    call _main
    jmp $