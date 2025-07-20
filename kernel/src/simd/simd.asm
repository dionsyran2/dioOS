[bits 64]
section .text
global _enable_avx

_no_sse_support:
    mov rdi, "SSE (Streaming SIMD Extension) is not supported by the cpu!"
    mov rsi, ""
    jmp $

_no_avx_support:
    mov rdi, "AVX is not supported by the cpu!"
    mov rsi, ""
    jmp $

_check_sse_avx_support:
    mov eax, 1
    cpuid
    test edx, 1 << 25
    jz _no_sse_support

    mov eax, 1
    cpuid
    test ecx, 1 << 28
    jz _no_avx_support
    ret

_enable_sse:
    ;now enable SSE and the like
    mov rax, cr0
    and ax, 0xFFFB		;clear coprocessor emulation CR0.EM
    or ax, 0x2			;set coprocessor monitoring  CR0.MP
    mov cr0, rax
    mov rax, cr4
    or rax, 3 << 9 | 1 << 18		;set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    mov cr4, rax
    ret

_enable_avx:
    call _check_sse_avx_support
    call _enable_sse

    fxsave [SavedFloats]

    push rax
    push rcx
    push rdx

    xor rcx, rcx

    xgetbv ;Load XCR0 register
    or eax, 7 ;Set AVX, SSE, X87 bits
    xsetbv ;Save back to XCR0

    pop rdx
    pop rcx
    pop rax
    ret


segment .data
align 16
SavedFloats: TIMES 512 db 0