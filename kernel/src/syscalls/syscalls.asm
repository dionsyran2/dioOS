[bits 64]
section .text
global syscall_int_handler
extern sys_free_cpp
extern sys_malloc_cpp
extern sys_open_cpp
extern sys_get_framebuffer_cpp
extern sys_free_pages_cpp
extern sys_request_pages_cpp
extern setInputEvent
extern sys_load_cpp
extern SetIDTGate
extern sys_set_tmr_val_cpp
extern sys_add_int_cpp
extern sys_add_elf_task_cpp
extern sys_task_sleep_cpp
extern sys_create_thread_cpp
extern sys_get_current_task_cpp

return_from_int:
    sti
    iretq

sys_load:
    call sys_load_cpp
    jmp return_from_int

sys_write:
    ;call
    jmp return_from_int

sys_open:
    call sys_open_cpp
    jmp return_from_int

sys_close:
    ;call
    jmp return_from_int

sys_malloc:
    call sys_malloc_cpp
    jmp return_from_int

sys_free:
    call sys_free_cpp
    jmp return_from_int

sys_getpid:
    ;call
    jmp return_from_int

sys_exit:
    ;call
    jmp return_from_int

sys_get_framebuffer:
    call sys_get_framebuffer_cpp
    jmp return_from_int

sys_request_pages:
    call sys_request_pages_cpp
    jmp return_from_int

sys_free_pages:
    call sys_free_pages_cpp
    jmp return_from_int

sys_set_int_val:
    call setInputEvent
    jmp return_from_int

sys_set_tmr_val:
    call sys_set_tmr_val_cpp
    jmp return_from_int

sys_add_int:
    call sys_add_int_cpp
    jmp return_from_int

sys_add_elf_task:
    call sys_add_elf_task_cpp
    jmp return_from_int

sys_task_sleep:
    call sys_task_sleep_cpp
    jmp return_from_int

sys_create_thread:
    call sys_create_thread_cpp
    jmp return_from_int

sys_get_current_task:
    call sys_get_current_task_cpp
    jmp return_from_int

syscall_int_handler:
    cmp RAX, 0; SYS READ
    je sys_load

    cmp RAX, 1; SYS WRITE
    je sys_write

    cmp RAX, 2 ; SYS OPEN
    je sys_open

    cmp RAX, 3 ; SYS CLOSE
    je sys_close

    cmp RAX, 4 ; malloc
    je sys_malloc

    cmp RAX, 5 ; free
    je sys_free

    cmp RAX, 6
    je sys_getpid

    cmp RAX, 7
    je sys_exit

    cmp RAX, 8
    je sys_get_framebuffer

    cmp RAX, 9
    je sys_request_pages 

    cmp RAX, 10
    je sys_free_pages 

    cmp RAX, 11
    je sys_set_int_val

    cmp RAX, 12
    je sys_set_tmr_val
    
    cmp RAX, 13
    je sys_add_int

    cmp RAX, 14
    je sys_add_elf_task

    cmp RAX, 15
    je sys_task_sleep

    cmp RAX, 16
    je sys_create_thread

    cmp RAX, 17
    je sys_get_current_task

    jmp return_from_int