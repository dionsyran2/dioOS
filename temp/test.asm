section .data
string db "hello world"

section .text

global _start
_start:
	mov rdi, 1
	mov rsi, string
	mov rdx, 11
	mov rax, 1
	syscall
	jmp $
