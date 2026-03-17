[bits 64]
section .text
global isr_stub_table
extern isr_common_stub
extern isr_dispatch
extern global_ptm_cr3

; ==== Common ISR stub that all vector-specific stubs jump to ====

isr_common_stub:
    ; Save general-purpose registers
    pop rdi   ; vector number

    ; Test the CS (Stored by the cpu) to check if we need to perform swapgs
    cmp qword [rsp + 136], 0x8
    je isr_common_stub_skip_swapgs_entry
    swapgs

isr_common_stub_skip_swapgs_entry:
    cld
    push rbp
    mov rbp, rsp
    and rsp, -16
    
    call isr_dispatch

    mov rsp, rbp
    pop rbp

    ; Same as before (to restore the gs pointer)
    cmp qword [rsp + 136], 0x8
    je isr_common_stub_skip_swapgs_exit
    swapgs

isr_common_stub_skip_swapgs_exit:
    ; Restore cr3
    pop rdi
    mov cr3, rdi
    
    ; Restore registers
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

        ; Swap cr3
        mov rdi, cr3
        push rdi

        mov rdi, qword [global_ptm_cr3]
        mov cr3, rdi

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
