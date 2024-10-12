section .text
global ap_startup

ap_startup:
    mov rsp, [ap_stack]  ; Set stack pointer
    extern ap_main
    call ap_main
    hlt
    jmp $