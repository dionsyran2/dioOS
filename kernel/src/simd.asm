[bits 64]
section .text
global _enable_avx
global _enable_sse
global _enable_avx512

; -----------------------------------------------------------------------------
; Helper: Check SSE (CPUID.1:EDX.25)
; -----------------------------------------------------------------------------
_check_sse_support:
    push rbx                ; Save RBX (Callee-saved, CPUID clobbers it)
    
    mov eax, 1
    cpuid
    test edx, 1 << 25
    
    pop rbx                 ; Restore RBX
    
    jz .no_sse
    mov rax, 1
    ret
.no_sse:
    xor rax, rax
    ret

; -----------------------------------------------------------------------------
; Helper: Check AVX (CPUID.1:ECX.28)
; -----------------------------------------------------------------------------
_check_avx_support:
    push rbx                ; Save RBX
    
    mov eax, 1
    cpuid
    test ecx, 1 << 28
    
    pop rbx                 ; Restore RBX
    
    jz .no_avx
    mov rax, 1
    ret
.no_avx:
    xor rax, rax
    ret

; -----------------------------------------------------------------------------
; Enable SSE
; Returns: 1 (True) on success, 0 (False) if unsupported
; -----------------------------------------------------------------------------
_enable_sse:
    call _check_sse_support
    test rax, rax
    jz .failed

    ; 1. Modify CR0 (Clear EM, Set MP)
    mov rax, cr0
    and ax, 0xFFFB      ; Clear Coprocessor Emulation (Bit 2)
    or ax, 0x2          ; Set Monitor Coprocessor (Bit 1)
    mov cr0, rax

    ; 2. Modify CR4 (Set OSFXSR, OSXMMEXCPT)
    mov rax, cr4
    or rax, (1 << 9) | (1 << 10)
    mov cr4, rax

    mov rax, 1          ; Explicit Success Return
    ret

.failed:
    xor rax, rax        ; Return False
    ret

; -----------------------------------------------------------------------------
; Enable AVX
; Returns: 1 (True) on success, 0 (False) if unsupported
; -----------------------------------------------------------------------------
_enable_avx:
    call _check_avx_support
    test rax, rax
    jz .failed

    ; 1. AVX requires SSE enabled first
    call _enable_sse
    test rax, rax       ; Check if SSE failed
    jz .failed

    ; 2. Enable XSAVE in CR4 (Bit 18)
    mov rax, cr4
    or rax, 1 << 18
    mov cr4, rax

    ; 3. Setup XCR0 (Enable X87, SSE, AVX states)
    push rcx            ; Save C++ caller registers just in case
    push rdx
    
    xor rcx, rcx        ; XCR0 is index 0
    xgetbv              ; Load current XCR0 into EDX:EAX
    or eax, 7           ; Set Bit 0 (x87), Bit 1 (SSE), Bit 2 (AVX)
    xsetbv              ; Write back to XCR0
    
    pop rdx
    pop rcx

    mov rax, 1          ; Explicit Success Return
    ret

.failed:
    xor rax, rax        ; Return False
    ret


; -----------------------------------------------------------------------------
; Helper: Check AVX-512 Foundation (CPUID.7.0:EBX.16)
; -----------------------------------------------------------------------------
_check_avx512_support:
    push rbx
    push rcx
    
    ; CPUID Leaf 7, Subleaf 0
    mov eax, 7
    xor ecx, ecx
    cpuid

    ; Check Bit 16 of EBX (AVX512F - Foundation)
    test ebx, 1 << 16
    
    pop rcx
    pop rbx
    
    jz .no_avx512
    mov rax, 1
    ret
.no_avx512:
    xor rax, rax
    ret

; -----------------------------------------------------------------------------
; Enable AVX-512
; Returns: 1 (True) on success, 0 (False) if unsupported
; -----------------------------------------------------------------------------
_enable_avx512:
    ; 1. AVX-512 requires standard AVX (and SSE) enabled first
    call _enable_avx
    test rax, rax
    jz .failed

    ; 2. Check if CPU physically supports AVX-512
    call _check_avx512_support
    test rax, rax
    jz .failed

    ; 3. Setup XCR0 for ZMM State
    ; We need to enable 3 new bits:
    ; Bit 5: Opmask (k0-k7 registers)
    ; Bit 6: ZMM_Hi256 (The upper 256 bits of ZMM0-ZMM15)
    ; Bit 7: Hi16_ZMM (The entirely new registers ZMM16-ZMM31)
    
    push rcx
    push rdx
    
    xor rcx, rcx        ; XCR0 index 0
    xgetbv              ; Load current XCR0
    
    or eax, 0xE0        ; OR in bits 5, 6, and 7 (1110 0000 in binary = 0xE0)
    
    xsetbv              ; Write back to XCR0
    
    pop rdx
    pop rcx

    mov rax, 1          ; Success
    ret

.failed:
    xor rax, rax
    ret

; -----------------------------------------------------------------------------
; int cpu_has_avx2(void);
; Returns 1 if AVX2 is supported, 0 otherwise.
; -----------------------------------------------------------------------------
global _cpu_has_avx2
_cpu_has_avx2:
    push rbx                ; RBX is callee-saved, we must preserve it!
    
    mov eax, 7              ; Request Extended Features
    xor ecx, ecx            ; Subleaf 0
    cpuid                   ; Output: EAX, EBX, ECX, EDX
    
    bt ebx, 5               ; Check Bit 5 of EBX (AVX2)
    setc al
    movzx eax, al
    
    pop rbx                 ; Restore RBX
    ret