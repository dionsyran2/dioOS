#include "calls.h"

extern "C" void* wm_sys_req_win_cpp(winCreateStruct_c* wcs){
    WM::WNDCLASS* wc = WM::GetClass(wcs->ClassName, wcs->thread);
    WM::window_t* win = WM::CreateWindow(wc, wcs->thread, nullptr, wcs->style, wcs->width, wcs->height, wcs->x, wcs->y, wcs->title);
    if (wcs->style & WS_TASKBAR) WM::CreateTaskbarEntry(win);
    return (void*)win; //compiler shenanigans
}

extern "C" void wm_sys_win_print_cpp(wchar_t* str, int size, uint32_t clr, void* win, int x, int y){
    int X = x;
    int Y = y;
    WM::window_t* window = (WM::window_t*)win;
    WM::fontRenderer->print(clr, str, X, Y, window->width, window->width, window->buffer, size);
    window->dirty_rects[window->amount_of_dirty_rects].x = X;
    window->dirty_rects[window->amount_of_dirty_rects].y = Y;
    window->dirty_rects[window->amount_of_dirty_rects].width = WM::fontRenderer->getTextLength(str, size);
    window->dirty_rects[window->amount_of_dirty_rects].height = size;
    window->amount_of_dirty_rects++;
    window->_Refresh = 1;
}

extern "C" int wm_get_str_width_cpp(wchar_t* str, int size){
    return WM::fontRenderer->getTextLength(str, size);
}

extern "C" time_t* wm_sys_get_time_cpp(){
    time_t* newTime = new time_t;
    memcpy(newTime, time, sizeof(time_t));
    return time;
}

extern "C" void wm_sys_add_wc_cpp(void* cl){
    WM::addClass((WM::WNDCLASS*)cl);
}

extern "C" void wm_sys_unregister_wp_cpp(void* cl){
    WM::removeClass((WM::WNDCLASS*) cl);
}

extern "C" int wm_sys_get_msg_cpp(void* msg, wchar_t* cn){
    WM::WNDCLASS* wc = WM::GetClass(cn, GetThread());
    while(wc->num_of_messages == 0) {
        wc = WM::GetClass(cn, GetThread());
        Sleep(200);
    }
    memcpy(msg, wc->messages[0], sizeof(WM::MSG));
    free(wc->messages[0]);
    for (int i = 0; i < wc->num_of_messages - 1; i++){
        wc->messages[i] = wc->messages[i + 1];
    }
    wc->num_of_messages--;
    
    return 1;
}

extern "C" void wm_sys_destroy_win_cpp(void* win){
    WM::window_t* window = (WM::window_t*)win;
    WM::closeWindow(window);
}

extern "C" void wm_sys_show_win_cpp(void* w){
    WM::window_t* win = (WM::window_t*)w;
    win->valid = 1;
}

extern "C" void* wm_sys_get_wc_cpp(wchar_t* cn, thread_t* thread){
    return (void*)WM::GetClass(cn, thread);
}