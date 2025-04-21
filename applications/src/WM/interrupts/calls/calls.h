#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../../rendering/wm/wm.h"

struct winCreateStruct_c{
    uint32_t style;
    wchar_t* ClassName;
    wchar_t* title;
    uint32_t rsv;
    int x;
    int y;
    uint32_t width;
    uint32_t height;
    void* parent;
    void* resv;
    thread_t* thread;
};

extern "C" void DWM_SYS_CALLS_HANDLER();

extern "C" void* wm_sys_req_win_cpp(winCreateStruct_c* wcs);
extern "C" int wm_get_str_width_cpp(wchar_t* str, int size);
extern "C" void wm_sys_win_print_cpp(wchar_t* str, int size, uint32_t clr, void* window, int x, int y);
extern "C" time_t* wm_sys_get_time_cpp();
extern "C" void wm_sys_add_wc_cpp(void* cl);
extern "C" void wm_sys_unregister_wp_cpp(void* cl);
extern "C" int wm_sys_get_msg_cpp(void* msg, wchar_t* win);
extern "C" void wm_sys_destroy_win_cpp(void* win);
extern "C" void wm_sys_show_win_cpp(void* w);
extern "C" void* wm_sys_get_wc_cpp(wchar_t* cn, thread_t* thread);