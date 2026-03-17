; interrupt_stubs.asm
extern exception_handler
global isr_stub_table
extern global_ptm_cr3

section .text

; Macro for exceptions with NO error code (e.g., Div by Zero)
%macro ISR_NOERR 1
global isr%1
isr%1:
    push 0                  ; Push Dummy Error Code
    push %1                 ; Push Interrupt Number
    jmp isr_common_stub
%endmacro

; Macro for exceptions WITH error code (e.g., Page Fault)
%macro ISR_ERR 1
global isr%1
isr%1:
    ; Error code is already pushed by CPU!
    push %1                 ; Push Interrupt Number
    jmp isr_common_stub
%endmacro

; Define the handlers
ISR_NOERR 0  ; Divide by Zero
ISR_NOERR 1  ; Debug
ISR_NOERR 2  ; NMI
ISR_NOERR 3  ; Breakpoint
ISR_NOERR 4  ; Overflow
ISR_NOERR 5  ; Bound Range
ISR_NOERR 6  ; Invalid Opcode
ISR_NOERR 7  ; Device Not Available
ISR_ERR   8  ; Double Fault (HAS ERROR CODE)
ISR_NOERR 9  ; Coprocessor Segment Overrun
ISR_ERR   10 ; Invalid TSS
ISR_ERR   11 ; Segment Not Present
ISR_ERR   12 ; Stack-Segment Fault
ISR_ERR   13 ; General Protection Fault
ISR_ERR   14 ; Page Fault
ISR_NOERR 16 ; x87 Floating-Point Exception
ISR_ERR   17 ; Alignment Check
ISR_NOERR 18 ; Machine Check
ISR_NOERR 19 ; SIMD Floating-Point Exception
ISR_NOERR 20 ; Virtualization Exception

; The Common Stub
isr_common_stub:
    cmp qword [rsp + 24], 0x08
    je common_stub_cont_call

    swapgs
common_stub_cont_call:
    cli
    push rax

    ; Swap Memory Mappings (You never know)
    mov rax, cr3
    push rax
    
    mov rax, qword [global_ptm_cr3]
    mov cr3, rax

    ; Continue pushing registers
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

    

    ; 2. Call C++ Handler
    cld
    mov rdi, rsp

    push rbp
    mov rbp, rsp
    and rsp, -16
    call exception_handler
    mov rsp, rbp
    pop rbp
    
    ; 3. Restore Registers
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
    ; restore cr3
    pop rax
    mov cr3, rax
    ; restore rax
    pop rax

    ; 4. Clean up the stack
    ; We pushed Interrupt Number (8 bytes) and Error Code (8 bytes)
    add rsp, 16 

    cmp qword [rsp + 8], 0x08
    je common_stub_cont_exit

    swapgs

common_stub_cont_exit:
    ; 5. Return from Interrupt
    iretq