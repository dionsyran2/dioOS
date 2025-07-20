[bits 64]
section .text
global APICTimerInt_ASM_Handler
extern APICTimerInt_Handler

global WM_MID_INTERRUPT
extern wm_mid_cpp_1
extern wm_mid_cpp_2

APICTimerInt_ASM_Handler:
    cli
    ; Save general-purpose registers
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

    ; Ensure stack is aligned before calling C++ function
    sub rsp, 8

    ; Pass interrupt frame (RSP at this point) to C++ handler
    mov rdi, rsp
    mov rsi, rbp
    call APICTimerInt_Handler

    ; Undo stack alignment after function call
    add rsp, 8

    ; Restore registers in reverse order
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