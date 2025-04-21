#include "libwm.h"
#include "cstr.h"
#include "math.h"

extern "C" bool exitFlag = false;


uint32_t window_t::getPix(uint32_t x, uint32_t y)
{
    return *(uint32_t *)((uint32_t *)buffer + x + (y * width));
}

void window_t::setPix(int x, int y, uint32_t color)
{
    *(uint32_t *)((uint32_t *)buffer + x + (y * width)) = color;
}

void window_t::clear(uint32_t color){
    uint32_t yEnd = (height - borderSize);
    uint32_t xEnd = width - borderSize;
    for (int Y = hiddenY; Y < yEnd; Y++){
        for (int X = borderSize; X < xEnd; X++){
            setPix(X, Y, color);
        }
    }
}

void window_t::Refresh(int x, int y, int width, int height){
    dirty_rects[amount_of_dirty_rects].x = x;
    dirty_rects[amount_of_dirty_rects].y = y;
    dirty_rects[amount_of_dirty_rects].width = width;
    dirty_rects[amount_of_dirty_rects].height = height;
    amount_of_dirty_rects++;

    _Refresh = 1;
}


window_t* CreateWindow(uint32_t style, wchar_t* ClassName, wchar_t* title, uint32_t rsv, int x, int y, uint32_t width, uint32_t height, window_t* parent, void* resv, thread_t* thread){
    wchar_t* cn = new wchar_t[strlen(ClassName) + 1];
    wchar_t* tl = new wchar_t[strlen(title) + 1];
    memcpy(cn, ClassName, strlen(ClassName) * sizeof(wchar_t));
    memcpy(tl, title, strlen(title) * sizeof(wchar_t));
    cn[strlen(ClassName) + 1] = L'\0';
    tl[strlen(title) + 1] = L'\0';
    winCreateStruct* wcs = new winCreateStruct;
    wcs->style = style;
    wcs->ClassName = cn;
    wcs->title = tl;
    wcs->rsv = rsv;
    wcs->x = x;
    wcs->y = y;
    wcs->width = width;
    wcs->height = height;
    wcs->parent = parent;
    wcs->thread = thread;

    window_t* win = crWinAsm(wcs);

    free(wcs);
    return win;
}

void TranslateMessage(MSG* msg){
    //translate keycode if necessary
}

void DispatchMessage(MSG* msg){
    msg->wc->WndProc(msg->win, msg->code, msg->param1, msg->param2);
}

void PostQuitMessage(wchar_t* cn, int code){
    GetClass(cn, GetThread())->_exitLoopFlag = true;
}

int DefWindowProc(window_t* win, uint32_t msg, uint64_t param1, uint64_t param2){
    switch (msg)
    {
        case WM_CLOSE:
            DestroyWindow(win);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(win->wc->ClassName, 0);
            return 0;
        
        case WM_PAINT:
            return 0;
            
        default:
            return 0;
    }
}


/* Functions to draw an anti aliased line. */
/* It uses wu's algorithm (and color interpolation to calculate colors for anti aliasing) */

void plot(uint32_t* framebuffer, int width, int height, int x, int y, float brightness, uint32_t color) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;

    uint32_t bg = framebuffer[y * width + x];

    uint8_t bg_r = (bg >> 16) & 0xFF;
    uint8_t bg_g = (bg >> 8) & 0xFF;
    uint8_t bg_b = bg & 0xFF;

    uint8_t fg_r = (color >> 16) & 0xFF;
    uint8_t fg_g = (color >> 8) & 0xFF;
    uint8_t fg_b = color & 0xFF;

    uint8_t r = fg_r * brightness + bg_r * (1.0f - brightness);
    uint8_t g = fg_g * brightness + bg_g * (1.0f - brightness);
    uint8_t b = fg_b * brightness + bg_b * (1.0f - brightness);

    framebuffer[y * width + x] = (r << 16) | (g << 8) | b;
}


void DrawLine(uint32_t* framebuffer, int width, int height, float x0, float y0, float x1, float y1, uint32_t color) {
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        swap(x0, y0);
        swap(x1, y1);
    }

    if (x0 > x1) {
        swap(x0, x1);
        swap(y0, y1);
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = dx == 0 ? 1 : dy / dx;

    // First endpoint
    float xEnd = roundf(x0);
    float yEnd = y0 + gradient * (xEnd - x0);
    float xGap = rfpart(x0 + 0.5f);
    int xPixel1 = xEnd;
    int yPixel1 = ipart(yEnd);

    if (steep) {
        plot(framebuffer, width, height, yPixel1, xPixel1, rfpart(yEnd) * xGap, color);
        plot(framebuffer, width, height, yPixel1 + 1, xPixel1, fpart(yEnd) * xGap, color);
    } else {
        plot(framebuffer, width, height, xPixel1, yPixel1, rfpart(yEnd) * xGap, color);
        plot(framebuffer, width, height, xPixel1, yPixel1 + 1, fpart(yEnd) * xGap, color);
    }

    float intery = yEnd + gradient;

    // Second endpoint
    xEnd = roundf(x1);
    yEnd = y1 + gradient * (xEnd - x1);
    xGap = fpart(x1 + 0.5f);
    int xPixel2 = xEnd;
    int yPixel2 = ipart(yEnd);

    if (steep) {
        plot(framebuffer, width, height, yPixel2, xPixel2, rfpart(yEnd) * xGap, color);
        plot(framebuffer, width, height, yPixel2 + 1, xPixel2, fpart(yEnd) * xGap, color);
    } else {
        plot(framebuffer, width, height, xPixel2, yPixel2, rfpart(yEnd) * xGap, color);
        plot(framebuffer, width, height, xPixel2, yPixel2 + 1, fpart(yEnd) * xGap, color);
    }

    // Main loop
    for (int x = xPixel1 + 1; x < xPixel2; ++x) {
        if (steep) {
            plot(framebuffer, width, height, ipart(intery), x, rfpart(intery), color);
            plot(framebuffer, width, height, ipart(intery) + 1, x, fpart(intery), color);
        } else {
            plot(framebuffer, width, height, x, ipart(intery), rfpart(intery), color);
            plot(framebuffer, width, height, x, ipart(intery) + 1, fpart(intery), color);
        }
        intery += gradient;
    }
}

WNDCLASS* GetClass(wchar_t* cn, thread_t* thread){
    int len = strlen(cn);
    wchar_t* str = new wchar_t[len];
    memcpy(str, cn, len);
    str[len] = L'\0';
    return GetClass_ASM(cn, thread);
}

int GetMessage(MSG* msg, wchar_t* cn, uint64_t rsv2, uint64_t rsv3){
    if (GetClass(cn, GetThread())->_exitLoopFlag == true){
        return 0;
    }
    return GetMessage_asm(msg, cn, rsv2, rsv3);
}