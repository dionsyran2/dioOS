[bits 64]
section .text

; __registers_t
; uint64_t cr3        + 0  
; uint64_t r15;       + 8
; uint64_t r14;       + 16
; uint64_t r13;       + 24
; uint64_t r12;       + 32
; uint64_t r11;       + 40
; uint64_t r10;       + 48
; uint64_t r9;        + 56 
; uint64_t r8;        + 64
; 
; uint64_t rbp;       + 72
; uint64_t rdi;       + 80
; uint64_t rsi;       + 88
; uint64_t rdx;       + 96
; uint64_t rcx;       + 104
; uint64_t rbx;       + 112
; uint64_t rax;       + 120
; 
; uint64_t rip;       + 128
; uint64_t CS;        + 136
; uint64_t rflags;    + 144
; uint64_t rsp;       + 152
; uint64_t ss;        + 160

; IRETQ stack
;+--------------------+
;|     Return SS      |
;+--------------------+
;|     Return RSP     |
;+--------------------+
;|    Return Flags    |
;+--------------------+
;|      Return CS     |
;+--------------------+
;|     Return RIP     |
;+--------------------+ <-- Current RSP


; RDI: __registers_t struct
global _execute_task
_execute_task:
    ; --- 1. Push the Interrupt Frame (Required for IRETQ) ---
    push qword [rdi + 160]  ; Push SS
    push qword [rdi + 152]  ; Push RSP
    push qword [rdi + 144]  ; Push RFLAGS
    push qword [rdi + 136]  ; Push CS
    push qword [rdi + 128]  ; Push RIP

    ; --- 2. Handle SWAPGS ---
    cmp qword [rdi + 136], 0x8 ; Check if CS != 0x08 (User Mode?)
    jnz swap_gs
    jmp continue_exec

swap_gs:
    swapgs

continue_exec:
    mov rax, qword [rdi + 120]
    push rax

    mov rax, qword [rdi]

    ; We skip RAX (handled above) and RDI (holds our pointer).
    mov r15, qword [rdi + 8]
    mov r14, qword [rdi + 16]
    mov r13, qword [rdi + 24]
    mov r12, qword [rdi + 32]
    mov r11, qword [rdi + 40]
    mov r10, qword [rdi + 48]
    mov r9,  qword [rdi + 56]
    mov r8,  qword [rdi + 64]
    mov rbp, qword [rdi + 72]
    mov rsi, qword [rdi + 88]
    mov rdx, qword [rdi + 96]
    mov rcx, qword [rdi + 104]
    mov rbx, qword [rdi + 112]

    mov rdi, qword [rdi + 80] 
    mov cr3, rax

    pop rax

    iretq