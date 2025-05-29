#pragma once
#include <stdint.h>
#include <stddef.h>
#include <libwm.h>
#include <ArrayList.h>
#include "widget.h"


typedef struct window_internal_t{
    uint8_t visible:1;
    int x;
    int y;
    uint32_t height;
    uint32_t width;
    uint32_t hiddenY; 
    uint8_t borderSize;

    wchar_t* title;
    void* buffer;

    uint64_t _zIndex;
    uint8_t _Focus:1;
    uint8_t _Refresh:1;
    rectangle_t dirty_rects[256];
    uint32_t amount_of_dirty_rects;
    WNDCLASS* wc;
    uint32_t style;

    bool movable;
    bool always_on_top;
    bool minimized;

    BOX closeBtn;

    ArrayList<WIDGET*>* widgets;

    uint32_t getPix(uint32_t x, uint32_t y);
    void putPix(uint32_t x, uint32_t y, uint32_t color);

    WIDGET* CreateWidget(WIDGET_TYPE type, const wchar_t* text, int identifier, int x, int y, uint32_t width, uint32_t height, uint32_t style);
} window_internal_t;