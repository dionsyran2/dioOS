#define MouseIcons

#include "mouse.h"
#include "icons.h"
#include "rendering.h"
#include "wclass.h"
#include "taskbar.h"
#include <math.h>

#define CURSOR_WIDTH 16
#define CURSOR_HEIGHT 24

uint32_t MouseCursorBuffer[CURSOR_WIDTH * CURSOR_HEIGHT];
uint32_t MouseCursorBufferAfter[CURSOR_WIDTH * CURSOR_HEIGHT];
uint32_t MouseCursorBMPBuffer[CURSOR_WIDTH * CURSOR_HEIGHT];


uint32_t mouseX, mouseY, mouse1Down, mouse2Down;
uint32_t prevMouseX, prevMouseY, prevMouse1Down, prevMouse2Down;
bool mouseDrawn = false;

void copy_bmp_to_buffer(uint32_t* dest_buffer, unsigned char* bmp_data) {
    uint32_t data_offset = *(uint32_t*)&bmp_data[10];  // BMP pixel data offset
    int width  = *(int32_t*)&bmp_data[18];
    int height = *(int32_t*)&bmp_data[22];
    int flip_vertically = height > 0;
    if (height < 0) height = -height;

    uint32_t* pixels = (uint32_t*)(bmp_data + data_offset);

    for (int y = 0; y < height; y++) {
        int src_y = flip_vertically ? (height - 1 - y) : y;
        for (int x = 0; x < width; x++) {
            dest_buffer[y * width + x] = pixels[src_y * width + x];
        }
    }
}

void blend_pixel(uint32_t& dst, uint32_t src) {
    uint8_t src_b = (src >>  0) & 0xFF;
    uint8_t src_g = (src >>  8) & 0xFF;
    uint8_t src_r = (src >> 16) & 0xFF;
    uint8_t src_a = (src >> 24) & 0xFF;

    if (src_a == 0) return; // fully transparent
    if (src_a == 255) { dst = src; return; } // fully opaque

    uint8_t dst_b = (dst >>  0) & 0xFF;
    uint8_t dst_g = (dst >>  8) & 0xFF;
    uint8_t dst_r = (dst >> 16) & 0xFF;

    // Normalize alpha
    float alpha = src_a / 255.0f;
    float inv_alpha = 1.0f - alpha;

    uint8_t out_r = (uint8_t)(src_r * alpha + dst_r * inv_alpha);
    uint8_t out_g = (uint8_t)(src_g * alpha + dst_g * inv_alpha);
    uint8_t out_b = (uint8_t)(src_b * alpha + dst_b * inv_alpha);
    uint8_t out_a = 255; // or blend alpha if you want

    dst = (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
}


void ClearMouse(){
    if (!mouseDrawn) return;

    int XMax = CURSOR_WIDTH;
    int YMax = CURSOR_HEIGHT;
    int DifferenceX = fb->width - prevMouseX;
    int DifferenceY = fb->height - prevMouseY;

    if (DifferenceX < XMax)
        XMax = DifferenceX;
    if (DifferenceY < YMax)
        YMax = DifferenceY;

    for (int Y = 0; Y < YMax; Y++){
        for (int X = 0; X < XMax; X++){
            uint32_t clr = MouseCursorBufferAfter[X + Y * CURSOR_WIDTH];
            
            if (GetPixInBB(prevMouseX + X, prevMouseY + Y) == clr)
            {
                PutPixInBB(prevMouseX + X, prevMouseY + Y, MouseCursorBuffer[X + Y * CURSOR_WIDTH]);
            }
        }
    }

}

void DrawMouse(){
    int XMax = CURSOR_WIDTH;
    int YMax = CURSOR_HEIGHT;
    int DifferenceX = fb->width - mouseX;
    int DifferenceY = fb->height - mouseY;

    if (DifferenceX < XMax)
        XMax = DifferenceX;
    if (DifferenceY < YMax)
        YMax = DifferenceY;


    if (!mouseDrawn){
        copy_bmp_to_buffer(MouseCursorBMPBuffer, mouse_cursor_bmp);
    }

    for (int Y = 0; Y < YMax; Y++){
        for (int X = 0; X < XMax; X++){
            uint32_t clr = MouseCursorBMPBuffer[X + (Y * CURSOR_WIDTH)];

            if ((clr & 0xFF000000) == 0) continue;

            int drawX = mouseX + X;
            int drawY = mouseY + Y;

            uint32_t dst = GetPixInBB(drawX, drawY) | 0xFF000000;
            MouseCursorBuffer[X + Y * CURSOR_WIDTH] = dst; // backup original

            blend_pixel(dst, clr);
            *(uint32_t*)((uint32_t*)backbuffer + drawX + (drawY * (fb->pitch / (fb->bpp / 8)))) = dst;

            MouseCursorBufferAfter[X + Y * CURSOR_WIDTH] = dst;
        }
    }
    
    
    mouseDrawn = true;
}



uint64_t lastClickTime = 0;
int lastClickX = 0;
int lastClickY = 0;
bool waitingForDoubleClick = false;

void onDoubleClick(){

}

void onClick(){
    uint64_t time = GetSystemTicks();
    int dx = abs(mouseX - lastClickX);
    int dy = abs(mouseY - lastClickY);

    if (waitingForDoubleClick && (time - lastClickTime) < 500 && dx < 5 && dy < 5){
        waitingForDoubleClick = false;

        return onDoubleClick();
    }else{
        lastClickX = mouseX;
        lastClickY = mouseY;
        lastClickTime = time;
        waitingForDoubleClick = true;

        TEXTBOX* prev = _focused_widget;
        _focused_widget = nullptr;

        if (prev != nullptr){
            prev->draw();
        }

        for (size_t i = 0; i < windows->length(); i++){
            window_internal_t* wn = windows->get(i);
            if (isMouseOnXBtn(mouseX, mouseY, wn)){
                MSG* msg = new MSG;
                msg->win = (window_t*)wn;
                msg->code = WM_CLOSE;
                msg->wc = wn->wc;
                SendMessageINT(msg, wn->wc);
                break;
            }

            if (wn->_Focus){
                for (size_t o = 0; o < wn->widgets->length(); o++){
                    WIDGET* widget = (WIDGET*)wn->widgets->get(o);

                    if (widget->parent->visible == false || widget->parent->minimized) continue;


                    if (widget->contains(mouseX - wn->x, mouseY - wn->y)){
                        widget->onClick();
                    }
                }
            } 
        }
    }
}

void onBtn1Down(){
    window_internal_t* win = getTopWindow(mouseX, mouseY);
    uint64_t _z = 0;
    if (win) _z = win->_zIndex;
    for (size_t i = 0; i < windows->length(); i++){
        window_internal_t* wn = windows->get(i);
        if (isMouseOnTitlebar(mouseX, mouseY, wn) && wn->_zIndex > _z){
            win = wn;
        }
    }

    if (win != nullptr){
        if (win->_Focus != true){
            focus(win);
        }else{
            if (isMouseOnTitlebar(mouseX, mouseY, win)){
                HandleMove(win, mouseX, mouseY, false);
            }
        }
    }

    if (!isWindowPixOnTop((window_internal_t*)start_menu, mouseX - win->x, mouseY - win->y)){
        start_menu->minimized = true;
        RedrawScreen(start_menu->x, start_menu->y - WIN_TITLE_BAR_HEIGHT, start_menu->width, start_menu->height + WIN_TITLE_BAR_HEIGHT);
    }
}

void onBtn1Up(){
    onClick();
    HandleMove(nullptr, mouseX, mouseY, true);
}


WIDGET* prevWidget = nullptr;


void UpdateMouse(uint32_t X, uint32_t Y, uint32_t b1Down, uint32_t b2Down){
    mouseX = X;
    mouseY = Y;
    mouse1Down = b1Down;
    mouse2Down = b2Down;

    if (mouse1Down == true && prevMouse1Down == false){
        onBtn1Down();
    }else if (mouse1Down == false && prevMouse1Down == true){
        onBtn1Up();
    }

    ClearMouse();
    prevMouseX = mouseX;
    prevMouseY = mouseY;
    prevMouse1Down = mouse1Down;
    prevMouse2Down = mouse2Down;
    DrawMouse();

    if (prevWidget != nullptr){
        if (!prevWidget->contains(X - prevWidget->parent->x, Y - prevWidget->parent->y)){
            prevWidget->draw();
            prevWidget = nullptr;
        }
    }

    for (size_t i = 0; i < windows->length(); i++){
        window_internal_t* win = windows->get(i);

        if (win->_Focus == false) continue;

        if (X > win->x && X < win->x + win->width && Y > win->y && Y < win->y + win->height){
            for (size_t o = 0; o < win->widgets->length(); o++){
                WIDGET* widget = (WIDGET*)win->widgets->get(o);
                if (widget->parent->visible == false || widget->parent->minimized) continue;
                if (widget->type == TYPE_BUTTON && widget->contains(X - win->x, Y - win->y)){
                    prevWidget = widget;
                    widget->draw();
                    break;
                }
            } 
        }
    }
}