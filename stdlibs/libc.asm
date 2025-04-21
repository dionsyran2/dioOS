[bits 64]
;SYSCALLS
global malloc
global free
global LoadFile
global OpenFile
global GetFramebuffer
global RequestPage
global RequestPages
global FreePage
global FreePages
global RegEvent
global SetTimeBuff
global AddInt
global RunELF
global Sleep
global CreateThread
global GetThread

malloc: ;malloc (size)
    mov rax, 4 ; syscall id
    int 0x80 ; trigger the syscall
    ret

free: ;free (address)
    mov rax, 5; syscall id
    int 0x80
    ret

OpenFile: ; OpenFile(drive, name, dir)
    mov rax, 2
    int 0x80
    ret

LoadFile:
    mov rax, 0
    int 0x80
    ret

GetFramebuffer: ; GetFramebuffer ()
    mov rax, 8
    int 0x80
    ret

RequestPage:
    mov rdi, 1
    mov rax, 9
    int 0x80
    ret

RequestPages:
    mov rax, 9
    int 0x80
    ret

FreePage:
    mov rsi, 1
    mov rax, 10
    int 0x80
    ret

FreePages:
    mov rax, 10
    int 0x80
    ret

RegEvent:
    mov rax, 11
    int 0x80
    ret

SetTimeBuff:
    mov rax, 12
    int 0x80
    ret

AddInt:
    mov rax, 13
    int 0x80
    ret

RunELF:
    mov rax, 14
    int 0x80
    ret

Sleep:
    mov rax, 15
    int 0x80
    ret

CreateThread:
    mov rax, 16
    int 0x80
    ret

GetThread:
    mov rax, 17
    int 0x80
    ret