[bits 64]

section .text
global DWM_SYS_CALLS_HANDLER

extern wm_sys_req_win_cpp
extern wm_sys_win_print_cpp
extern wm_get_str_width_cpp
extern wm_sys_get_time_cpp
extern wm_sys_add_wc_cpp
extern wm_sys_get_msg_cpp
extern wm_sys_destroy_win_cpp
extern wm_sys_unregister_wp_cpp
extern wm_sys_show_win_cpp
extern wm_sys_get_wc_cpp
extern wm_create_widget_cpp
extern wm_redraw_widget_cpp;
extern wm_get_widget_text_cpp
extern wm_unblock_taskbar

wm_sys_req_win:
    call wm_sys_req_win_cpp
    iretq

wm_sys_win_print:
    call wm_sys_win_print_cpp
    iretq

wm_get_str_width:
    call wm_get_str_width_cpp
    iretq

wm_sys_get_time:
    call wm_sys_get_time_cpp
    iretq

wm_sys_add_wc:
    call wm_sys_add_wc_cpp
    iretq

wm_sys_get_msg:
    call wm_sys_get_msg_cpp
    iretq

wm_sys_destroy_win:
    call wm_sys_destroy_win_cpp
    iretq

wm_sys_unregister_wp:
    call wm_sys_unregister_wp_cpp
    iretq

wm_sys_show_win:
    call wm_sys_show_win_cpp
    iretq

wm_sys_get_wc:
    call wm_sys_get_wc_cpp
    iretq

wm_create_widget:
    call wm_create_widget_cpp
    iretq

wm_redraw_widget:
    call wm_redraw_widget_cpp
    iretq
wm_get_widget_text:
    call wm_get_widget_text_cpp
    iretq

wm_unblock_taskbar_asm:
    call wm_unblock_taskbar
    iretq

DWM_SYS_CALLS_HANDLER:
    sti
    cmp rax, 1
    je wm_sys_req_win

    cmp rax, 2
    je wm_sys_win_print

    cmp rax, 3
    je wm_get_str_width
    
    cmp rax, 4
    je wm_sys_get_time

    cmp rax, 5
    je wm_sys_add_wc

    cmp rax, 6
    je wm_sys_get_msg

    cmp rax, 7
    je wm_sys_destroy_win

    cmp rax, 8
    je wm_sys_unregister_wp

    cmp rax, 9
    je wm_sys_show_win

    cmp rax, 10
    je wm_sys_get_wc

    cmp rax, 11
    je wm_create_widget

    cmp rax, 12
    je wm_redraw_widget_cpp
    
    cmp rax, 13
    je wm_get_widget_text

    cmp rax, 14
    je wm_unblock_taskbar_asm

    sti
    iretq