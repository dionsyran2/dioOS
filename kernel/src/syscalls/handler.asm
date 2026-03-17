[bits 64]
section .text

extern local_krnl_stack_offset
extern local_user_stack_scratch_offset
extern handle_syscall
extern global_ptm_cr3

global syscall_entry
syscall_entry:
    swapgs

    mov [gs:8], rax 

    mov rax, [local_user_stack_scratch_offset]
    mov [gs:rax], rsp

    mov rax, [local_krnl_stack_offset]
    mov rsp, [gs:rax]


    push qword ((4 * 8) | 3)

    mov rax, [local_user_stack_scratch_offset]
    push qword [gs:rax]      ; Push the value stored at GS:[RAX]

    push r11                 ; syscall saves RFLAGS in R11

    push qword ((3 * 8) | 3)

    push rcx                 ; syscall saves RIP in RCX

    
    mov rax, [gs:8]
    push rax                 ; Struct offset: rax

    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    
    push r8
    push r9
    push r10                 ; This holds the 4th Syscall Argument
    push r11                 ; This holds the old RFLAGS
    push r12
    push r13
    push r14
    push r15


    ; Save current CR3 (User Page Table)
    mov rax, cr3
    push rax

    mov rax, qword [global_ptm_cr3]
    mov cr3, rax

    sti
    
    mov rdi, rsp

    sub rsp, 8
    
    call handle_syscall
    
    add rsp, 8
    cli


    pop rax
    mov cr3, rax

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

    swapgs
    iretq