[bits 64]
extern global_ptm_cr3

section .text

%macro push_registers 0
    ; Push all of the registers, in reverse order (for __registers_t, defined in cpu.h)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro pop_registers 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; ==============================================================================
; MACRO: ISR_HANDLER
; Arguments:
;   %1 = Assembly Label Name (e.g., apic_timer_handler)
;   %2 = C++ Function Name   (e.g., apic_timer_handler_cpp)
; ==============================================================================
%macro ISR_HANDLER 2
    extern %2               ; Declare the C++ function as external
    global %1               ; Export the assembly label
    
    %1:
        cli
        ; --- Check if there was a privilage change ---
        ; CS is at RSP+8.
        cmp qword [rsp + 8], 0x08
        je %%skip_swapgs_entry
        swapgs
    %%skip_swapgs_entry:

        push_registers

        ; Swap Memory Mappings (You never know)
        mov rdi, cr3
        push rdi

        mov rdi, qword [global_ptm_cr3]
        mov cr3, rdi

        ; --- Call C++ ---
        mov rdi, rsp        ; rdi: Pointer to __registers_t
        
        cld
        push rbp
        mov rbp, rsp
        and rsp, -16
        
        call %2
        
        mov rsp, rbp
        pop rbp

        ; Restore the mappings
        pop rdi
        mov cr3, rdi

        pop_registers

        cmp qword [rsp + 8], 0x08
        je %%skip_swapgs_exit
        swapgs
    %%skip_swapgs_exit:
        
        iretq
%endmacro

; ==============================================================================
; HANDLER DEFINITIONS
; ==============================================================================

ISR_HANDLER apic_timer_handler,         apic_timer_handler_cpp
ISR_HANDLER task_scheduler_swap_task, task_scheduler_swap_task_cpp
ISR_HANDLER spurious_interrupt_handler, spurious_interrupt_handler_cpp
