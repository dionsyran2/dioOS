[bits 64]
section .text
global syscall_entry
extern syscall_entry_cpp
extern next_syscall_stack
extern user_syscall_stack

syscall_entry:
    mov cr2, rax ; use cr2 as a scratch register to save the original rax value
    mov rax, [next_syscall_stack] ; load the kernel stack in rax (next_syscall_stack is changed by the scheduler for each task)
    xchg rax, rsp ; exchange rax with rsp to load the kstack in rsp and save the user stack in rax

    mov [user_syscall_stack], rax ; save the user stack
    push rbp ; save the rbp

    mov rax, cr2 ; restore the original value of cr2
    mov rbp, rsp ; set the rbp to the top of the kstack

    ; Save all of the registers
    push rbx
    push rcx
    push rdx
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
    push rax ; push rax to the stack, to use it as the 6th argument in c

    mov rcx, r10 ; move r10 into rcx to comply with the x86_64 calling convention
    call syscall_entry_cpp ; call the cpp handler
    add rsp, 8 ; remove the 6th arg (rax)

    ; restore all of the registers
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
    pop rdx
    pop rcx
    pop rbx
    pop rbp


    ; To clear things up here for anyone reading
    ; here used to be an 'o64 sysret' instruction... but on my
    ; AMD Ryzen 7 5700x (and by extension, VMware running on it),
    ; it would somehow corrupt the stack... makes no sense but it
    ; did that... (on intel cpus it worked fine). After 2 days of
    ; troubleshooting, i tried using iretq... this works! and you
    ; know what they say, if it aint broke, dont fuck with it


    mov cr2, rax ; save rax (use cr2 again as a scratch register)
    
    mov ax, (3 * 8) | 3 ; ss
	mov ds, ax
	mov es, ax 

	push (3 * 8) | 3 ; ds

    mov rax, [user_syscall_stack] ; load the user stack in rax

	push rax ; push the stack address
	push r11 ; push the flags (syscall preserved them in r11)
	push (4 * 8) | 3 ; cs

	push rcx ; push the return address (syscall preserved it in rcx)
    mov rax, cr2 ; reset the value of rax (the return value)
	iretq ; return to the caller