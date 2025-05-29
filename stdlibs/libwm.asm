[bits 64]
section .text

global crWinAsm
global GetTextWidth
global print
global GetTime
global RegisterClass
global UnregisterClass
global GetMessage_asm
global DestroyWindow
global ShowWindow
global GetClass_ASM
global CreateWidgetAsm
global RedrawWidget
global GetText
global wm_sys_unblock_taskbar

extern exitFlag

;WINDOW MANAGER

crWinAsm:
    mov rax, 1
    int 0x7F
    ret

print:
    mov rax, 2
    int 0x7F
    ret

GetTextWidth:
    mov rax, 3
    int 0x7F
    ret

GetTime:
    mov rax, 4
    int 0x7F
    ret

RegisterClass:
    mov rax, 5
    int 0x7F
    ret

exitFlagSet:
    mov rax, 0 ; return 0
    ret

GetMessage_asm:
    mov al, [rel exitFlag]
    cmp al, 1
    je exitFlagSet

    mov rax, 6
    int 0x7F
    ret

DestroyWindow:
    mov rax, 7
    int 0x7F
    ret

UnregisterClass:
    mov rax, 8
    int 0x7F
    ret

ShowWindow:
    mov rax, 9
    int 0x7F
    ret

GetClass_ASM:
    mov rax, 10
    int 0x7F
    ret

CreateWidgetAsm:
    mov rax, 11
    int 0x7F
    ret

RedrawWidget:
    mov rax, 12
    int 0x7F
    ret

GetText:
    mov rax, 13
    int 0x7F
    ret

wm_sys_unblock_taskbar:
    mov rax, 14
    int 0x7F
    ret