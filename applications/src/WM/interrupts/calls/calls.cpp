#include "calls.h"
#include "../../rendering/wm/wm.h"
#include "../../rendering/wm/wclass.h"
#include "../../rendering/wm/taskbar.h"
#include "../../rendering/wm/rendering.h"

extern "C" void* wm_sys_req_win_cpp(winCreateStruct_c* wcs){
    WNDCLASS* wclass = FindClassINT(wcs->ClassName, wcs->thread);
    return (void*)CreateWindowInternal(wclass, GetThread(), (window_t*)wcs->parent, wcs->style, wcs->width, wcs->height, wcs->x, wcs->y, wcs->title);
}

extern "C" void wm_sys_win_print_cpp(wchar_t* str, int size, uint32_t clr, void* win, int x, int y){
    int X = x;
    int Y = y;
    window_t* window = (window_t*)win;
    renderer->print(clr, str, X, Y, window->width, window->width, window->buffer, size);
    
    window->dirty_rects[window->amount_of_dirty_rects].x = X;
    window->dirty_rects[window->amount_of_dirty_rects].y = Y;
    window->dirty_rects[window->amount_of_dirty_rects].width = renderer->getTextLength(str, size);
    window->dirty_rects[window->amount_of_dirty_rects].height = size;
    window->amount_of_dirty_rects++;
    window->_Refresh = 1;
}

extern "C" int wm_get_str_width_cpp(wchar_t* str, int size){
    return renderer->getTextLength(str, size);
}

extern "C" time_t* wm_sys_get_time_cpp(){
    return time;
}

extern "C" void wm_sys_add_wc_cpp(void* cl){
    RegisterClassINT((WNDCLASS*)cl);
}

extern "C" void wm_sys_unregister_wp_cpp(void* cl){
    UnRegisterClassINT((WNDCLASS*) cl);
}

extern "C" int wm_sys_get_msg_cpp(void* msg, wchar_t* cn){
    WNDCLASS* wc = FindClassINT(cn, GetThread());
    while(wc->num_of_messages == 0) {
        wc = FindClassINT(cn, GetThread());
        Sleep(200);
    }

    memcpy(msg, wc->messages[0], sizeof(MSG));

    //free(wc->messages[0]);

    for (int i = 0; i < wc->num_of_messages - 1; i++){
        wc->messages[i] = wc->messages[i + 1];
    }

    wc->num_of_messages--;
    
    return 1;
}

extern "C" void wm_sys_destroy_win_cpp(void* win){
    window_t* window = (window_t*)win;
    DestroyWindowInternal(window);
}

extern "C" void wm_sys_show_win_cpp(void* w){
    ((window_t*)w)->visible = true;
    RedrawScreen(0, 0, fb->width, fb->height);
}

extern "C" void* wm_sys_get_wc_cpp(wchar_t* cn, thread_t* thread){
    return (void*)FindClassINT(cn, thread);
}


extern "C" void* wm_create_widget_cpp(CreateWidgetStruct_c* strct){
    WIDGET_TYPE tp;
    switch(strct->type){
        case WIDGET_TYPE_EX::TYPE_BUTTON_EX:
            tp = WIDGET_TYPE::TYPE_BUTTON;
            break;
        case WIDGET_TYPE_EX::TYPE_TEXTBOX_EX:
            tp = WIDGET_TYPE::TYPE_TEXTBOX;
            break;
    }
    void* ad = (void*)((window_internal_t*)strct->parent)->CreateWidget(tp, strct->text, strct->identifier, strct->x, strct->y, strct->width, strct->height, strct->style);
    return ad;
}

extern "C" void wm_redraw_widget_cpp(WIDGET* widget){
    //((BUTTON*)widget)->draw();
}

extern "C" wchar_t* wm_get_widget_text_cpp(WIDGET* widget){
    return ((TEXTBOX*)widget)->text;
}

extern "C" void wm_unblock_taskbar(){
    wait_to_init_taskbar = false;
}