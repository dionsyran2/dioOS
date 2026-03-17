[bits 16]
; 1. Entry Point
jmp short start_trampoline
nop

; 2. Data Block (Aligned to +0x10)
times 16-($-$$) db 0
trampoline_data:
    var_pml4:      dq 0        ; Offset +0
    var_stack:     dq 0        ; Offset +8
    var_entry:     dq 0        ; Offset +16

start_trampoline:
    cli

    ; Setup Stack & Segments
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x1000  ; Set Stack to top of the 4KB page (Safety)

    ; Calculate Physical Base Address
    xor eax, eax
    mov ax, cs
    shl eax, 4      ; EAX = 0x1000 (Physical Base)
    mov ebx, eax    ; CRITICAL: Save Base in EBX for 32-bit mode!

    ; Patch & Load GDT
    add eax, gdt_table      ; EAX = Linear Address of GDT
    mov [gdt_desc + 2], eax ; Patch the descriptor
    lgdt [gdt_desc]

    ; Enable Protected Mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    
    ; Push Code Segment (0x08)
    push word 0x08
    
    ; Calculate Target Offset (Base + Label)
    ; Target = 0x1000 (EBX) + protected_mode_offset
    mov eax, ebx
    add eax, protected_mode
    
    ; Push 32-bit Instruction Pointer
    db 0x66         ; Operand size override (Push 32-bit EAX)
    push eax

    ; "Return" to the address we just pushed
    db 0x66         ; Operand size override (32-bit Return)
    retf
[bits 32]
protected_mode:
    mov ax, 0x10    ; Data Segment
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; -------------------------------------------------------------
    ; 1. Recover Data Pointer (using preserved EBX Base)
    ; -------------------------------------------------------------
    mov esi, ebx             ; ESI = 0x1000 (Base)
    add esi, trampoline_data ; ESI = 0x1010 (Physical Data Addr)
    
    ; Load CR3 (PML4)
    mov eax, [esi]
    mov cr3, eax

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Enable Long Mode (EFER.LME)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable Paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

   
    ; We need to write the 6-byte pointer [Offset:Segment] to memory,
    ; then JMP FAR using that memory location.
    
    ; Calculate Physical Address of 'long_mode' label
    mov eax, ebx
    add eax, long_mode      ; EAX = Physical Address of long_mode
    
    ; Calculate Physical Address of 'jmp_ptr' variable
    ; We need to write the pointer HERE.
    mov edi, ebx
    add edi, jmp_ptr        ; EDI = Physical Address of jmp_ptr struct

    ; Write the Far Pointer to [EDI]
    mov [edi], eax          ; Write 32-bit Offset
    mov word [edi+4], 0x18  ; Write 16-bit Segment (64-bit Code)

    ; Execute Far Jump using the pointer at [EDI]
    jmp far [edi]

    ; Storage for the jump pointer (Located in code, skipped over)
    align 4
jmp_ptr:
    dd 0
    dw 0

[bits 64]
long_mode:
    ; RDI is the first argument for C++ functions
    ; Move our data pointer (ESI) to RDI
    mov edi, esi 
    
    ; Load Stack
    mov rsp, [rdi + 8]  ; var_stack
    
    ; Load Entry Point
    mov rax, [rdi + 16] ; var_entry
    
    ; Jump to C++!
    call rax
    hlt

; =================================================================
; GDT Table
; =================================================================
align 16
gdt_table:
    dq 0x0000000000000000 ; 0x00: Null
    dq 0x00CF9A000000FFFF ; 0x08: 32-bit Code
    dq 0x00CF92000000FFFF ; 0x10: 32-bit Data
    dq 0x00AF9A000000FFFF ; 0x18: 64-bit Code
gdt_end:

gdt_desc:
    dw gdt_end - gdt_table - 1
    dd 0 ; Runtime patched