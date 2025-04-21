[bits 16]
section .text
global ap_start
global ap_start_end
ALIGN 0x1000
ap_start:
    cli
    lgdt [0x1000 + (gdt.gdt_pointer - ap_start)]
    mov al, 0x41
    mov dx, 0x03F8
    out dx, al


    mov eax, cr0
    or eax, 1
    mov cr0, eax


    mov eax,  (0x1000 + (protected_mode - ap_start))
    jmp 0x08:(0x1000 + (protected_mode - ap_start))
    hlt


protected_mode:
    mov eax, 0x1234
    jmp $

[bits 16]
align 16
gdt:
    dq 0                       ; Null descriptor (always required)

.code_segment:
    dw 0xFFFF                  ; Limit (low 16 bits)
    dw 0x0000                  ; Base (low 16 bits)
    db 0x00                    ; Base (middle 8 bits)
    db 0x9A                    ; Access byte: Executable, Readable, Present
    db 0xCF                    ; Flags: 4K Granularity, 32-bit protected mode
    db 0x00                    ; Base (high 8 bits)

.data_segment:
    dw 0xFFFF                  ; Limit (low 16 bits)
    dw 0x0000                  ; Base (low 16 bits)
    db 0x00                    ; Base (middle 8 bits)
    db 0x92                    ; Access byte: Writable, Present
    db 0xCF                    ; Flags: 4K Granularity, 32-bit protected mode
    db 0x00                    ; Base (high 8 bits)

.gdt_pointer:
    dw gdt.gdt_pointer - gdt - 1    ; GDT size (16-bit limit)
    dd gdt                      ; GDT base address (32-bit address)
ap_start_end:
    