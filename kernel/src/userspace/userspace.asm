[bits 64]
global jump_usermode
global flush_tss
flush_tss:
	mov ax, (5 * 8) | 0 ; fifth 8-byte selector, symbolically OR-ed with 0 to set the RPL (requested privilege level).
	ltr ax
	ret

jump_usermode:
	sti
	mov ax, (3 * 8) | 3 ; ring 3 data with bottom 2 bits set for ring 3
	mov ds, ax
	mov es, ax 

	push (3 * 8) | 3 ; data selector
	push rdi ; provided rsp
	pushf ; eflags
	push (4 * 8) | 3 ; code selector (ring 3 code with bottom 2 bits set for ring 3)

	push rsi ; instruction address to return to
	mov rdi, rdx ;move 3rd arg (parameter) to the 1st arg
	iretq

