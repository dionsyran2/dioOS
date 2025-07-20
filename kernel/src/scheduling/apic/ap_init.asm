[bits 16]
section .rodata
global ap_start
global ap_start_end
global ap_protected_mode_entry
global ap_gdt_start
global ap_gdt_end

%define GDT_OFFSET ap_start_end - ap_start + 10

ALIGN 0x1000

ap_start:
    cli

    in al, 0x92
    or al, 0x02    ; set A20 enable bit
    and al, 0xFE   ; clear reset bit (bit 0)
    out 0x92, al
    
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    lgdt [cs:GDT_OFFSET]

    mov eax, cr0
    or eax, 1 ; set PE (Protection Enable) bit in CR0 (Control Register 0)
    mov cr0, eax

    mov ax, cs

    jmp dword 0x08:ap_protected_mode_entry

    hlt
ap_start_end:

ap_gdt_start:
gdt_null:                      ; Null descriptor (mandatory)
    dq 0

gdt_code:                      ; Code Segment: base=0, limit=4GB, type=exec/read
    dw 0xFFFF                  ; Limit (15:0)
    dw 0x0000                  ; Base (15:0)
    db 0x00                    ; Base (23:16)
    db 0x9A                    ; Access: present, ring 0, executable, readable
    db 0xCF                    ; Flags: 4K granularity, 32-bit, limit (19:16)
    db 0x00                    ; Base (31:24)

gdt_data:                      ; Data Segment: base=0, limit=4GB, type=read/write
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92                    ; Access: present, ring 0, data, writable
    db 0xCF                    ; Flags: 4K granularity, 32-bit
    db 0x00

ap_gdt_end:

[bits 32]
global ap_long_mode_gdt_address
global ap_krnl_page_table
global ap_krnl_stack

ap_long_mode_gdt_address:
dd 0x00000000

ap_krnl_page_table:
dd 0x00000000

ap_krnl_stack:
dq 0x0000000000000000


SECTION .text

ap_protected_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov eax, [ap_krnl_page_table]
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    mov eax, [ap_long_mode_gdt_address]

    lgdt [eax]

    jmp 0x08:ap_long_mode_entry


[bits 64]

extern _apmain
extern _enable_avx
ap_long_mode_entry:
    mov rdi, [ap_krnl_stack]
    mov rsp, rdi

    call _enable_avx

    call _apmain
    jmp $