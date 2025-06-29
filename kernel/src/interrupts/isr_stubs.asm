[bits 64]
section .text
global isr_stub_table
extern isr_common_stub
extern isr_dispatch

; ==== Common ISR stub that all vector-specific stubs jump to ====

isr_common_stub:
    ; Save general-purpose registers
    pop rdi   ; vector number
    
    sub rsp, 8
    
    mov rsi, rsp            ; full stack frame
    call isr_dispatch

    add rsp, 8              ; undo alignment

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    sti
    iretq

; ==== Define 256 stubs dynamically ====
isr_stub_table:
%assign i 0
%rep 256
    global isr_stub_ %+ i
    isr_stub_ %+ i:
        cli
        push rax
        push rcx
        push rdx
        push rbx
        push rbp
        push rsi
        push rdi
        push r8
        push r9
        push r10
        push r11
        push r12
        push r13
        push r14
        push r15
        push i

        jmp isr_common_stub
%assign i i+1
%endrep

; ==== Now define the table of pointers ====
section .data
global isr_stub_table_ptrs
isr_stub_table_ptrs:
%assign i 0
%rep 256
    dq isr_stub_ %+ i
%assign i i+1
%endrep
