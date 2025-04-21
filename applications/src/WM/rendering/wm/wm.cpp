#define MouseIcons
#include "../../main.h"
time_t* time;

namespace WM
{
    uint8_t MouseCursor[] = {
        0b11111111,
        0b11100000,
        0b11111111,
        0b10000000,
        0b11111110,
        0b00000000,
        0b11111100,
        0b00000000,
        0b11111000,
        0b00000000,
        0b11110000,
        0b00000000,
        0b11100000,
        0b00000000,
        0b11000000,
        0b00000000,
        0b11000000,
        0b00000000,
        0b10000000,
        0b00000000,
        0b10000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
    };

    framebuffer_t *fb;
    size_t sizeOfFB;
    void *background;
    void *backbuffer;
    uint32_t MouseCursorBuffer[16 * 16];
    uint32_t MouseCursorBufferAfter[16 * 16];

    window_t** windows;
    window_t* taskbar;
    uint32_t num_of_windows = 0; // 0x1000 / sizeof(window_t);
    uint8_t HIGHEST_Z_INDEX = 0;
    uint32_t mouseX, mouseY, mouse1Down, mouse2Down, mouseUpdated;
    uint32_t prevMouseX, prevMouseY, prevMouse1Down, prevMouse2Down;
    TTF *fontRenderer;
    WNDCLASS** wclasses;
    uint16_t num_of_wclasses;


    void addClass(WNDCLASS* cl){
        wclasses[num_of_wclasses] = cl;
        num_of_wclasses++;
    }

    void removeClass(WNDCLASS* cl){
        int index = -1;
        for (int i = 0; i < num_of_wclasses; i++){
            if (wclasses[i]->thread->pid == cl->thread->pid){
                index = i;
                break;
            }
        }

        if (index == -1) return;

        for (int i = index; i < num_of_wclasses; i++){
            wclasses[i] = wclasses[i + 1];
        }
        
        num_of_wclasses--;
        wclasses[num_of_wclasses] = NULL;
    }

    WNDCLASS* GetClass(wchar_t* CN, thread_t* thread){
        for (int i = 0; i < num_of_wclasses; i++){
            WNDCLASS* wc = wclasses[i];
            if (wc->thread->pid == thread->pid && strcmp(CN, wc->ClassName) == 0) return wc;
        }
    }


// LCG parameters (these are commonly used values)
#define MODULUS 2147483648 // 2^31
#define MULTIPLIER 1103515245
#define INCREMENT 12345

    static unsigned int seed = 14412; // Global seed variable
    void srand_custom(unsigned int initialSeed)
    {
        seed = initialSeed;
    };

    // Function to generate a random number
    unsigned int rand_custom()
    {
        seed = (MULTIPLIER * seed + INCREMENT) % MODULUS;
        return seed;
    }

    // Function to generate a random hex color with alpha
    unsigned int randHexColor()
    {
        // Generate random values for Alpha, Red, Green, Blue using custom rand
        unsigned int alpha = rand_custom() % 256; // Random value for Alpha (0-255)
        unsigned int red = rand_custom() % 256;   // Random value for Red (0-255)
        unsigned int green = rand_custom() % 256; // Random value for Green (0-255)
        unsigned int blue = rand_custom() % 256;  // Random value for Blue (0-255)

        // Combine ARGB values into a single hex color (0xAARRGGBB)
        unsigned int hexColor = (alpha << 24) | (red << 16) | (green << 8) | blue;

        return hexColor;
    }

    bool widget_t::Clicked(){
        uint32_t xStart = x + parent->x;
        uint32_t xEnd = xStart + width;

        uint32_t yStart = y + parent->y;
        uint32_t yEnd = yStart + height;

        if ((style & WS_DISABLED) == 0 && parent->_Focus == 1 && mouseX >= xStart && mouseX <= xEnd && mouseY >= yStart && mouseY <= yEnd && prevMouse1Down == 1 && mouse1Down == 0)
        {
            return true;
        }
        return false;
    }

    widget_t* window_t::CreateWidget(WIDGET_TYPE type, wchar_t* text, uint32_t style, int x, int y, int width, int height){
        widget_t* widget = new widget_t;
        
        widget->x = x;
        widget->y = y;
        widget->width = width;
        widget->height = height;
        widget->style = style;
        widget->type = type;

        switch (type){
            case WIDGET_TYPE::Button:
                widget->text = text;
                break;
            case WIDGET_TYPE::Label:
                widget->text = text;
                break;
            case WIDGET_TYPE::TextBox:
                widget->placeHolder = text;
                break;
        }

        widgets[num_of_widgets] = widget;
        num_of_widgets++;

        return widget;
    }

    void widget_t::Render(){
        if (style & WS_VISIBLE == 0) return;
        switch(type){
            case WIDGET_TYPE::Button:{
                /* Render the background */
                uint8_t border = style & WS_NOBORDER ? 0 : 2;
                for (uint32_t py = y; py < y + height; py++)
                {
                    for (uint32_t px = x; px < x + width; px++)
                    {
                        uint32_t color = 0xC0C0C0;
                        // Correct border condition checks
                        if (py < y + border || py >= (y + height - border) ||
                            px < x + border || px >= (x + width - border))
                        {
                            color = 0;
                        }
                        parent->setPix(px, py, color);
                    }
                }
                
                for (int Y = 0; Y < 2; Y++){
                    for (int X = 0; X < width; X++){
                        uint32_t clr = 0xffffff;
                        if (Y == 1) clr = 0xDEDEDE;
                        parent->setPix(x + X, y + Y, clr);
                    }
                }

                for (int Y = 0; Y < height; Y++){
                    for (int X = 0; X < 2; X++){
                        uint32_t clr = 0xffffff;
                        if (X == 1 && Y != 0) clr = 0xDEDEDE;
                        parent->setPix(x + X, y + Y, clr);
                    }
                }

                for (int Y = 0; Y < 2; Y++){
                    for (int X = 0; X < width + 1; X++){
                        uint32_t clr = 0x0;
                        
                        if (X == 0 && Y == 1) continue;
                        if (y == 1) clr = 0x7B7B7B;
                        parent->setPix(x + X, y + height - Y, clr);
                    }
                }

                for (int Y = 0; Y < height; Y++){
                    for (int X = 0; X < 2; X++){
                        uint32_t clr = 0x0;
                        if (X == 1 && Y == 0) continue;
                        if (X == 1 && Y != 0) clr = 0x7B7B7B;
                        parent->setPix(x + width - X, y + Y, clr);
                    }
                }

                uint8_t textSize = 16;
                uint32_t txtHeight = height - 4;
                uint8_t textWidth = WM::fontRenderer->getTextLength(text, txtHeight);
                uint32_t tx = x + (width - textWidth) / 2;
                uint32_t ty = y + (height - txtHeight) / 2;
                fontRenderer->print(0x0, text, tx, ty, parent->width, parent->width, parent->buffer, txtHeight);
                break;
            }

            case WIDGET_TYPE::Label:{
                /* Render the background */
                uint8_t border = style & WS_NOBORDER ? 0 : 2;

                for (uint32_t py = y; py < y + height; py++)
                {
                    for (uint32_t px = x; px < x + width; px++)
                    {
                        uint32_t color = 0xC0C0C0;
                        // Correct border condition checks
                        if (py < y + border || py >= (y + height - border) ||
                            px < x + border || px >= (x + width - border))
                        {
                            color = 0;
                        }
                        parent->setPix(px, py, color);
                    }
                }
                
                for (int Y = 0; Y < 2; Y++){
                    for (int X = 0; X < width; X++){
                        uint32_t clr = 0xffffff;
                        if (Y == 1) clr = 0xDEDEDE;
                        parent->setPix(x + X, y + Y, clr);
                    }
                }

                for (int Y = 0; Y < height; Y++){
                    for (int X = 0; X < 2; X++){
                        uint32_t clr = 0xffffff;
                        if (X == 1 && Y != 0) clr = 0xDEDEDE;
                        parent->setPix(x + X, y + Y, clr);
                    }
                }

                for (int Y = 0; Y < 2; Y++){
                    for (int X = 0; X < width + 1; X++){
                        uint32_t clr = 0x0;
                        
                        if (X == 0 && Y == 1) continue;
                        if (y == 1) clr = 0x7B7B7B;
                        parent->setPix(x + X, y + height - Y, clr);
                    }
                }

                for (int Y = 0; Y < height; Y++){
                    for (int X = 0; X < 2; X++){
                        uint32_t clr = 0x0;
                        if (X == 1 && Y == 0) continue;
                        if (X == 1 && Y != 0) clr = 0x7B7B7B;
                        parent->setPix(x + width - X, y + Y, clr);
                    }
                }

                uint8_t textSize = 16;
                uint32_t txtHeight = height - 4;
                uint8_t textWidth = WM::fontRenderer->getTextLength(text, txtHeight);
                uint32_t tx = x + (width - textWidth) / 2;
                uint32_t ty = y + (height - txtHeight) / 2;
                fontRenderer->print(0x0, text, tx, ty, parent->width, parent->width, parent->buffer, txtHeight);
                break;
            }

        }
    }

    button_t *window_t::createButton(uint32_t width, uint32_t height, uint32_t x, uint32_t y, wchar_t *text)
    {
        button_t *btn = new button_t;
        memset(btn, 0, sizeof(button_t));
        //buttons[numOfButtons] = btn;
        //numOfButtons++;
        btn->width = width;
        btn->height = height;
        btn->x = x;
        btn->y = y;
        btn->text = text;
        btn->parent = this;
        btn->backgroundColor = 0xC0C0C0;
        return btn;
    }

    void window_t::removeButton(button_t* btn){
        if (!btn) return;
    
        int index = -1;
        /*for (int i = 0; i < numOfButtons; i++) {
            if (1/*buttons[i] == btn) {
                index = i;
                break;
            }
        }*/
    
        if (index == -1) return;
    
        /*for (int i = index; i < numOfButtons - 1; i++) {
            buttons[i] = buttons[i + 1];
        }*/
    }

    uint32_t window_t::getPix(uint32_t x, uint32_t y)
    {
        return *(uint32_t *)((uint32_t *)buffer + x + (y * width));
    }

    void window_t::setPix(int x, int y, uint32_t color)
    {
        *(uint32_t *)((uint32_t *)buffer + x + (y * width)) = color;
    }

    void RemoveWindow(window_t* win) {
        if (!win) return;
    
        win->CloseWindow();
    
        int index = -1;
        for (int i = 0; i < num_of_windows; i++) {
            if (windows[i] == win) {
                index = i;
                break;
            }
        }
    
        if (index == -1) return;
    
        for (int i = index; i < num_of_windows - 1; i++) {
            windows[i] = windows[i + 1];
        }
    
        num_of_windows--;
        windows[num_of_windows] = nullptr; // Optional: Clear last pointer
    
        free(win);
    }


    void closeWindow(window_t* window){
        MSG* msg = new MSG;
        msg->win = window;
        msg->code = WM_DESTROY;
        msg->wc = window->wc;
        SendMessage(msg);
        RemoveTaskbarEntry(window);
        RemoveWindow(window);
    }

    uint32_t interpolateColor(uint32_t color1, uint32_t color2, int offset, int width)
    {
        if (offset < 0) offset = 0;
        if (offset > width - 1) offset = width - 1;

        float t = (float)offset / (width - 1);

        uint8_t r1 = (color1 >> 16) & 0xFF;
        uint8_t g1 = (color1 >> 8)  & 0xFF;
        uint8_t b1 = (color1 >> 0)  & 0xFF;

        uint8_t r2 = (color2 >> 16) & 0xFF;
        uint8_t g2 = (color2 >> 8)  & 0xFF;
        uint8_t b2 = (color2 >> 0)  & 0xFF;

        uint8_t r = r1 + (r2 - r1) * t;
        uint8_t g = g1 + (g2 - g1) * t;
        uint8_t b = b1 + (b2 - b1) * t;

        return (r << 16) | (g << 8) | b;
    }

    void window_t::refreshTitleBar()
    {

        uint32_t borderColor = 0xC0C0C0;

        for (int y = borderSize; y < hiddenY; y++)
        {
            for (int x = borderSize; x < width - borderSize; x++)
            {
                uint32_t color = interpolateColor(0x00007B, 0x0884CE, x, width);
                if (/*y == 0 || */ y >= (hiddenY - borderSize) /* || x == 0 || x == width - 1*/)
                {
                    color = borderColor;
                }

                setPix(x, y, color);
            }
        }

        uint32_t btn_width = 16;
        uint32_t btn_height = 13;
        uint32_t btnx = width - btn_width - borderSize - 3;
        uint32_t btny = borderSize + 2;

        if (closeBtn == nullptr){
            closeBtn = CreateWidget(WIDGET_TYPE::Button, L"X", 0, btnx, btny, btn_width, btn_height);
        }
        uint16_t closeBtn[] = {0x0000, 0x0000, 0x0000, 0x0c30, 0x0660, 0x03c0, 0x0180, 0x03c0, 0x0660, 0x0c30, 0x0000, 0x0000, 0x0000, 0x0000};
        for (int y = 0; y < btn_height; y++){
            for (int x = 0; x < btn_width; x++){
                uint32_t clr = 0xC0C0C0;
                if ((closeBtn[y] >> (15 - x)) & 1) clr = 0x0;
                setPix(btnx + x, btny + y, clr);
            }
        }
        for (int y = 0; y < 2; y++){
            for (int x = 0; x < btn_width; x++){
                uint32_t clr = 0xffffff;
                if (y == 1) clr = 0xDEDEDE;
                setPix(btnx + x, btny + y, clr);
            }
        }

        for (int y = 0; y < btn_height; y++){
            for (int x = 0; x < 2; x++){
                uint32_t clr = 0xffffff;
                if (x == 1 && y != 0) clr = 0xDEDEDE;
                setPix(btnx + x, btny + y, clr);
            }
        }

        for (int y = 0; y < 2; y++){
            for (int x = 0; x < btn_width + 1; x++){
                uint32_t clr = 0x0;
                
                if (x == 0 && y == 1) continue;
                if (y == 1) clr = 0x7B7B7B;
                setPix(btnx + x, btny + btn_height - y, clr);
            }
        }

        for (int y = 0; y < btn_height; y++){
            for (int x = 0; x < 2; x++){
                uint32_t clr = 0x0;
                if (x == 1 && y == 0) continue;
                if (x == 1 && y != 0) clr = 0x7B7B7B;
                setPix(btnx + btn_width - x, btny + y, clr);
            }
        }


        fontRenderer->print(0xFFFFFF, title, 5, borderSize + 1, width, width, buffer, hiddenY - borderSize - 4);
        dirty_rects[amount_of_dirty_rects].height = 0;
        dirty_rects[amount_of_dirty_rects].width = width;
        dirty_rects[amount_of_dirty_rects].x = 0;
        dirty_rects[amount_of_dirty_rects].y = hiddenY;
        amount_of_dirty_rects++;
    }

    void window_t::setName(wchar_t* name){
        title = name;
        refreshTitleBar();
    }

    void button_t::render()
    {
        /* Render the background */
        for (uint32_t py = y; py < y + height; py++)
        {
            for (uint32_t px = x; px < x + width; px++)
            {
                uint32_t color = backgroundColor;
                // Correct border condition checks
                if (py < y + border || py >= (y + height - border) ||
                    px < x + border || px >= (x + width - border))
                {
                    color = borderColor;
                }
                parent->setPix(px, py, color);
            }
        }
        
        for (int Y = 0; Y < 2; Y++){
            for (int X = 0; X < width; X++){
                uint32_t clr = 0xffffff;
                if (Y == 1) clr = 0xDEDEDE;
                parent->setPix(x + X, y + Y, clr);
            }
        }

        for (int Y = 0; Y < height; Y++){
            for (int X = 0; X < 2; X++){
                uint32_t clr = 0xffffff;
                if (X == 1 && Y != 0) clr = 0xDEDEDE;
                parent->setPix(x + X, y + Y, clr);
            }
        }

        for (int Y = 0; Y < 2; Y++){
            for (int X = 0; X < width + 1; X++){
                uint32_t clr = 0x0;
                
                if (X == 0 && Y == 1) continue;
                if (y == 1) clr = 0x7B7B7B;
                parent->setPix(x + X, y + height - Y, clr);
            }
        }

        for (int Y = 0; Y < height; Y++){
            for (int X = 0; X < 2; X++){
                uint32_t clr = 0x0;
                if (X == 1 && Y == 0) continue;
                if (X == 1 && Y != 0) clr = 0x7B7B7B;
                parent->setPix(x + width - X, y + Y, clr);
            }
        }

        uint8_t textSize = 16;
        uint32_t txtHeight = height - 4;
        uint8_t textWidth = WM::fontRenderer->getTextLength(text, txtHeight);
        uint32_t tx = x + (width - textWidth) / 2;
        uint32_t ty = y + (height - txtHeight) / 2;
        fontRenderer->print(0x0, text, tx, ty, parent->width, parent->width, parent->buffer, txtHeight);
    }

    void UnfocusAll()
    {
        for (uint32_t i = 0; i < num_of_windows; i++)
        {
            window_t* win = windows[i];
            win->_Focus = 0;
        }
    }

    void PutPixInBB(uint32_t x, uint32_t y, uint32_t color)
    {
        *(uint32_t *)((uint32_t *)backbuffer + x + (y * (fb->common.framebuffer_pitch / (fb->common.framebuffer_bpp / 8)))) = color;
    }

    uint32_t GetPixInBB(uint32_t x, uint32_t y)
    {
        return *(uint32_t *)((uint32_t *)backbuffer + x + (y * (fb->common.framebuffer_pitch / (fb->common.framebuffer_bpp / 8))));
    }

    void PutPixInFB(uint32_t x, uint32_t y, uint32_t color)
    {
        *(uint32_t *)((uint32_t *)fb->common.framebuffer_addr + x + (y * (fb->common.framebuffer_pitch / (fb->common.framebuffer_bpp / 8)))) = color;
    }

    uint32_t GetPixInFB(uint32_t x, uint32_t y)
    {
        return *(uint32_t *)((uint32_t *)fb->common.framebuffer_addr + x + (y * (fb->common.framebuffer_pitch / (fb->common.framebuffer_bpp / 8))));
    }


    void CopyBBToFB(int X, int Y, uint32_t width, uint32_t height){
        uint32_t yEnd = Y + height;
        uint32_t xEnd = X + width;
        for (int y = Y; y < yEnd; y++){
            for (int x = X; x < xEnd; x++){
                PutPixInFB(x, y, GetPixInBB(x, y));
            }
        }
    }

    void drawWinBorder(window_t* win){
        for (int y = 0; y < win->borderSize; y++){
            for (int x = 0; x < win->width; x++){
                uint32_t color = 0xBDBDBD;
                if (y == 0) color = 0xDEDEDE;
                if (y == 1) color = 0xFFFFFF;
                win->setPix(x, y, color);
            }
        }

        for (int y = 0; y < win->height; y++){
            for (int x = 0; x < win->borderSize; x++){
                uint32_t color = 0xBDBDBD;
                if (x == 1 || y == 1) color = 0xFFFFFF;
                if (x == 0 || y == 0) color = 0xDEDEDE;

                win->setPix(x, y, color);
            }
        }

        for (int y = 0; y < win->borderSize; y++){
            for (int x = 0; x < win->width; x++){
                if ((x == 1 && y > 1) || (x == 0 && y > 0)) continue;
                uint32_t color = 0xBDBDBD;
                if (y == 1) color = 0x7B7B7B;
                if (y == 0) color = 0x0;

                win->setPix(x, win->height - y - 1, color);
            }
        }

        for (int y = 0; y < win->height; y++){
            for (int x = 0; x < win->borderSize; x++){
                uint32_t color = 0xBDBDBD;
                if (y == 0 && x != 0) continue;
                if (y == 1 && x > 1) continue;

                if ((x == 1 || (y == win->height - 2))) color = 0x7B7B7B;
                if (x == 0 || y == 0 || y == win->height -1) color = 0x0;

                win->setPix(win->width - x - 1, y, color);
            }
        }
        
    }

    window_t *FindTopWindowAtPos(int x, int y)
    {
        window_t *highest = nullptr;
        if ((x >= taskbar->x && x < (taskbar->x + taskbar->width)) && y >= taskbar->y && y < (taskbar->y + taskbar->height))
        {
            return taskbar;
        }
        for (uint32_t i = 0; i < num_of_windows; i++)
        {
            window_t *win = windows[i];
            if (win->valid == 0 || win->minimized == true) continue;
            if ((x >= win->x && x < (win->x + win->width)) && y >= win->y && y < (win->y + win->height))
            {
                if (highest == nullptr)
                {
                    highest = win;
                }
                else
                {
                    if (highest->_zIndex < win->_zIndex)
                    {
                        highest = win;
                    }
                }
            }
        }
        return highest;
    }

    void Refresh()
    {

        for (uint32_t y = 0; y < fb->common.framebuffer_height; y++)
        {
            for (uint32_t x = 0; x < fb->common.framebuffer_width; x++)
            {

                window_t *win = FindTopWindowAtPos(x, y);
                uint32_t color = 0;
                if (win == nullptr)
                {
                    color = *(uint32_t *)((uint32_t *)background + x + (y * (fb->common.framebuffer_pitch / (fb->common.framebuffer_bpp / 8))));
                }
                else
                {
                    color = *(uint32_t *)((uint32_t *)win->buffer + (x - win->x) + ((y - win->y) * (win->width)));
                }

                *(uint32_t *)((uint32_t *)backbuffer + x + (y * (fb->common.framebuffer_pitch / (fb->common.framebuffer_bpp / 8)))) = color;
            }
        }

        //memcpy((void *)fb->common.framebuffer_addr, backbuffer, sizeOfFB);
    }

    void Refresh(int xStart, int yStart, uint32_t width, uint32_t height)
    {
        if (xStart < 0) xStart = 0;
        if (yStart < 0) yStart = 0;
        uint32_t yEnd = yStart + height;
        uint32_t xEnd = xStart + width;

        if (yEnd > fb->common.framebuffer_height) yEnd = fb->common.framebuffer_height;
        if (xEnd > fb->common.framebuffer_width) xEnd = fb->common.framebuffer_width;
        for (uint32_t y = yStart; y < yEnd; y++)
        {
            for (uint32_t x = xStart; x < xEnd; x++)
            {

                window_t *win = FindTopWindowAtPos(x, y);
                uint32_t color = 0;
                if (win == nullptr)
                {
                    color = *(uint32_t *)((uint32_t *)background + x + (y * (fb->common.framebuffer_pitch / (fb->common.framebuffer_bpp / 8))));
                }
                else
                {
                    color = *(uint32_t *)((uint32_t *)win->buffer + (x - win->x) + ((y - win->y) * (win->width)));
                }

                *(uint32_t *)((uint32_t *)backbuffer + x + (y * (fb->common.framebuffer_pitch / (fb->common.framebuffer_bpp / 8)))) = color;
            }
        }

        //memcpy((void *)fb->common.framebuffer_addr, backbuffer, sizeOfFB);
    }

    window_t *CreateWindow(WNDCLASS* cl, thread_t* thread, window_t* parent, uint32_t style, uint32_t width, uint32_t height, uint32_t x, uint32_t y, wchar_t* title){
        window_t *win = new window_t;
        memset(win, 0, sizeof(window_t));
        windows[num_of_windows] = win;
        num_of_windows++;

        win->width = width;
        win->height = height;
        win->x = x;
        win->y = y;
        win->borderSize = 4;
        win->hiddenY = 22 + win->borderSize;
        win->buffer = RequestPages((sizeOfFB / 0x1000) + 1);
        win->_zIndex = HIGHEST_Z_INDEX;
        win->movable = true;
        win->title = title;
        win->wc = cl;
        win->parent = parent;
        win->style = style;
        
        HIGHEST_Z_INDEX++;
        UnfocusAll();
        //win->valid = 1;
        win->_Focus = 1;
        //win->numOfButtons = 0;

        win->closeBtn = nullptr;
        for (int y = 0; y < height; y++){
            for (int x = 0; x < width; x++){
                win->setPix(x, y, 0xC0C0C0);   
            }
        }
        win->refreshTitleBar();
        drawWinBorder(win);

        win->dirty_rects[win->amount_of_dirty_rects].x = 0;
        win->dirty_rects[win->amount_of_dirty_rects].y = 0;
        win->dirty_rects[win->amount_of_dirty_rects].width = width;
        win->dirty_rects[win->amount_of_dirty_rects].height = height;
        win->amount_of_dirty_rects++;
        win->_Refresh = 1;

        Refresh(win->x, win->y, win->width, win->height);
        //CopyBBToFB(win->x, win->y, win->width, win->height);
        return win;
    }

    window_t *CreateWindow(WNDCLASS* cl, thread_t* thread, uint32_t width, uint32_t height, uint32_t x, uint32_t y, wchar_t* title){
        CreateWindow(cl, thread, nullptr, NULL, width, height, x, y, title);
    }

    void FillBuffer(void *buffer, uint32_t width, uint32_t height, uint8_t bpp, uint32_t color)
    {
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                *(uint32_t *)((uint32_t *)buffer + x + (y * (fb->common.framebuffer_pitch / (bpp / 8)))) = color;
            }
        }
    }

    void window_t::CloseWindow(){
        FreePages(buffer, (sizeOfFB / 0x1000) + 1);
        valid = 0;
        Refresh(x, y, width, height);
        return;
    }

    bool isWindowPixOnTop(window_t *window, uint32_t x, uint32_t y)
    {
        if (FindTopWindowAtPos(window->x + x, window->y + y) == window)
        {
            return true;
        }

        return false;
    }

    void UpdateWindowOnScreen(window_t *window)
    {
        for (uint32_t i = 0; i < window->amount_of_dirty_rects; i++)
        {

            rectangle_t rect = window->dirty_rects[i];
            for (uint32_t y = rect.y; y < rect.y + rect.height; y++)
            {
                for (uint32_t x = rect.x; x < rect.x + rect.width; x++)
                {
                    if (!isWindowPixOnTop(window, x, y) || ((x + window->x) > mouseX && (x + window->x) < (mouseX + 16) && (y + window->y) > mouseY && (y + window->y) < mouseY + 16))
                    {
                        continue;
                    }
                    uint32_t color = window->getPix(x, y);
                    *(uint32_t *)((uint32_t *)backbuffer + (window->x + x) + ((window->y + y) * (fb->common.framebuffer_pitch / (fb->common.framebuffer_bpp / 8)))) = color;
                }
            }
            //CopyBBToFB(window->x + rect.x, window->y + rect.y, rect.width, rect.height);
        }
        window->_Refresh = 0;
        window->amount_of_dirty_rects = 0;
    }
    void UpdateScreen()
    {
        /* Check the taskbar */

        /* WINDOWS */

        for (uint32_t z = 0; z < HIGHEST_Z_INDEX; z++)
        {
            for (int i = 0; i < num_of_windows; i++)
            {
                window_t *window = windows[i];
                if (window->valid == 0)
                    continue;
                if (window->_zIndex != z)
                    continue;
                if (window->minimized == true)
                    continue;

                if (window->_Refresh == 1)
                {
                    UpdateWindowOnScreen(window);
                }
            }
        }
    }

    bool button_t::isHovering()
    {
        if (mouseX > (x + parent->x) && mouseX < (x + parent->x) + width && mouseY > (y + parent->y) && mouseY < (y + parent->y) + height && mouse1Down == 0 && isWindowPixOnTop(parent, mouseX, mouseY))
        {
            return true;
        }
        return false;
    }

    bool button_t::clicked()
    {
        uint32_t xStart = x + parent->x;
        uint32_t xEnd = xStart + width;

        uint32_t yStart = y + parent->y;
        uint32_t yEnd = yStart + height;

        if (disabled == 0 && parent->_Focus == 1 && mouseX >= xStart && mouseX <= xEnd && mouseY >= yStart && mouseY <= yEnd && mouse1Down == 1)
        {
            return true;
        }
        return false;
    }

    bool mouseDrawn = false;

    void ClearMouse()
    {
        if (!mouseDrawn)
            return;

        int XMax = 10;
        int YMax = 16;
        int DifferenceX = fb->common.framebuffer_width - prevMouseX;
        int DifferenceY = fb->common.framebuffer_height - prevMouseY;

        if (DifferenceX < XMax)
            XMax = DifferenceX;
        if (DifferenceY < YMax)
            YMax = DifferenceY;

        int rowStride = 2;
        for (int Y = 0; Y < YMax; Y++)
        {
            for (int X = 0; X < XMax; X++)
            {
                int Byte = Y * rowStride + (X / 8);
                int Bit = X % 8;
                if ((MouseOutline[Byte] & (0b10000000 >> Bit)))
                {
                    if (GetPixInBB(prevMouseX + X, prevMouseY + Y) == MouseCursorBufferAfter[X + Y * 16])
                    {
                       PutPixInBB(prevMouseX + X, prevMouseY + Y, MouseCursorBuffer[X + Y * 16]);
                    }
                }
            }
        }
        for (int Y = 0; Y < YMax; Y++)
        {
            for (int X = 0; X < XMax; X++)
            {
                int Byte = Y * rowStride + (X / 8);
                int Bit = X % 8;
                if ((MouseFill[Byte] & (0b10000000 >> Bit)))
                {
                    if (GetPixInBB(prevMouseX + X, prevMouseY + Y) == MouseCursorBufferAfter[X + Y * 16])
                    {
                       PutPixInBB(prevMouseX + X, prevMouseY + Y, MouseCursorBuffer[X + Y * 16]);
                    }
                }
            }
        }
        //CopyBBToFB(prevMouseX, prevMouseY, XMax, YMax);
    }
    void DrawMouse()
    {
        ClearMouse();
        prevMouseX = mouseX;
        prevMouseY = mouseY;
        prevMouse1Down = mouse1Down;
        prevMouse2Down = mouse2Down;
        
        int XMax = 10;
        int YMax = 16;
        int DifferenceX = fb->common.framebuffer_width - mouseX;
        int DifferenceY = fb->common.framebuffer_height - mouseY;

        if (DifferenceX < XMax)
            XMax = DifferenceX;
        if (DifferenceY < YMax)
            YMax = DifferenceY;

        
        int rowStride = 2;
        
        /* OUTLINE (White) */
        for (int Y = 0; Y < YMax; Y++)
        {
            for (int X = 0; X < XMax; X++)
            {
                int Byte = Y * rowStride + (X / 8);
                int Bit = X % 8;
                if ((MouseOutline[Byte] & (0b10000000 >> Bit)))
                {
                    uint32_t color = 0xFFFFFF;

                    MouseCursorBuffer[X + Y * 16] = GetPixInBB(mouseX + X, mouseY + Y);
                    PutPixInBB(mouseX + X, mouseY + Y, color);
                    MouseCursorBufferAfter[X + Y * 16] = GetPixInBB(mouseX + X, mouseY + Y);
                }
            }
        }



        /* FILL (Black) */
        for (int Y = 0; Y < YMax; Y++)
        {
            for (int X = 0; X < XMax; X++)
            {
                int Byte = Y * rowStride + (X / 8);
                int Bit = X % 8;
                if ((MouseFill[Byte] & (0b10000000 >> Bit)) != 0 && (MouseOutline[Byte] & (0b10000000 >> Bit)) == 0)
                {
                    uint32_t color = 0x0;

                    MouseCursorBuffer[X + Y * 16] = GetPixInBB(mouseX + X, mouseY + Y);
                    PutPixInBB(mouseX + X, mouseY + Y, color);
                    MouseCursorBufferAfter[X + Y * 16] = GetPixInBB(mouseX + X, mouseY + Y);
                }
            }
        }

        //CopyBBToFB(mouseX, mouseY, XMax, YMax);
        mouseDrawn = true;
    }

    void focus(window_t* window){
        UnfocusAll();
        window->_Focus = 1;
        window->_zIndex = HIGHEST_Z_INDEX;
        HIGHEST_Z_INDEX++;
        Refresh(window->x, window->y, window->width + 1, window->height + 1);
    }
    void drawBorder(int X, int Y, int width, int height){
        uint32_t color = 0x4a4a4a;
        for (int y = Y; y < Y + height; y++){ // left border
            if (y < 0 || y > fb->common.framebuffer_height - 1) continue;
            if (X < 0 || X > fb->common.framebuffer_width - 1) continue;
            PutPixInBB(X, y, color);
        }

        for (int y = Y; y < Y + height; y++){ // right border
            if (y < 0 || y > fb->common.framebuffer_height - 1) continue;
            if (X + width < 0 || X + width > fb->common.framebuffer_width - 1) continue;
            PutPixInBB(X + width, y, color);
        }

        for (int x = X; x < X + width; x++){ // top border
            if (Y < 0 || Y > fb->common.framebuffer_height - 1) continue;
            if (x < 0 || x > fb->common.framebuffer_width - 1) continue;
            PutPixInBB(x, Y, color);
        }

        for (int x = X; x < X + width; x++){
            if (Y + width < 0 || Y + width > fb->common.framebuffer_height - 1) continue;
            if (x < 0 || x > fb->common.framebuffer_width - 1) continue;
            PutPixInBB(x, Y + height, color);
        }
    }

    void clearBorder(int X, int Y, int width, int height){
        Refresh(X, Y, width, 1); //top
        Refresh(X, Y + height, width, 1); //bottom
        Refresh(X, Y, 1, height); //left
        Refresh(X + width, Y, 1, height); //right
    }

    int targX = 0;
    int targY = 0;
    uint64_t started = 0;
    window_t *targWin = nullptr;

    int HandleMoveBorder(window_t* window, int mX, int mY, bool released){
        
        if (window) if (!window->movable) return 0;
        if (released){
            if (targWin != nullptr){
                started = 0;
                int prevX = targWin->x;
                int prevY = targWin->y;

                targWin->x = targX;
                targWin->y = targY;

                focus(targWin); // focus & refresh the new position
                Refresh(prevX, prevY, targWin->width, targWin->height); // refresh the old position
                
                targWin = nullptr;
                started = 0;
            }
            return 0;
        }

        if (window != nullptr && targWin != window && started == 0 && mouseY < (window->y + window->hiddenY)){
            targWin = window;
        }
        
        if (targWin != nullptr)
        {
            if (started == 0){
                started = 1;
                targX = targWin->x;
                targY = targWin->y;
            }
            else
            {
                if (mouseUpdated == 0) return 0;
                clearBorder(targX, targY, targWin->width, targWin->height);

                //CopyBBToFB(targX, targY, targWin->width, targWin->height);
                targX += (mX - prevMouseX);
                targY += (mY - prevMouseY);
            }
            
            //memcpy(BBCopy, backbuffer, sizeOfFB);

            drawBorder(targX, targY, targWin->width, targWin->height);
            DrawMouse();

            //CopyBBToFB(targX, targY, targWin->width, targWin->height);

            //memcpy((void*)fb->common.framebuffer_addr, backbuffer, sizeOfFB);
            return 0;
        }
        return -1;
        
    }

    void** event;

    bool is_leap(int year) {
        return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    }
    
    int days_in_month(int year, int month) {
        static const int days[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
        if (month == 2 && is_leap(year)) return 29;
        return days[month - 1];
    }

    
    void updateTime(uint64_t timestamp){
        uint64_t secs = timestamp;
        time->second = secs % 60;
        secs /= 60;
        time->minute = secs % 60;
        secs /= 60;
        time->hour = secs % 24;
        secs /= 24;

        // Now `secs` is number of days since 1970-01-01
        int year = 1970;
        while (true) {
            int days_in_year = is_leap(year) ? 366 : 365;
            if (secs >= days_in_year) {
                secs -= days_in_year;
                ++year;
            } else {
                break;
            }
        }
        time->year = year;

        int month = 1;
        while (true) {
            int dim = days_in_month(year, month);
            if (secs >= dim) {
                secs -= dim;
                ++month;
            } else {
                break;
            }
        }
        time->month = month;
        time->day = secs + 1;
    }

    void HandleEvent(){
        EVENT_KB* kb = (EVENT_KB*)*event;
        *event = kb->nextEvent;
        switch(kb->type){
            case 1:{ // Keyboard
                break;
            }
            case 2:{ // Mouse
                EVENT_MOUSE* mouse = (EVENT_MOUSE*)kb;
                mouseX = mouse->x;
                mouseY = mouse->y;
                mouse1Down = mouse->data & 1;
                mouse2Down = (mouse->data & (1 << 2)) >> 2;
                mouseUpdated = 1;
                break;
            }
            default:{ 
                break;
            }
        }
        free(kb);
    }

    void ErrorPopup(wchar_t* error){
        window_t* win = CreateWindow(GetClass(L"WNDMGR", tsk), tsk,nullptr, NULL, 400, 100, (fb->common.framebuffer_width - 400) / 2, (fb->common.framebuffer_height - 100) / 2, L"ERROR");
        focus(win);
        uint8_t textSize = 16;
        uint32_t txtHeight = 20;
        uint8_t textWidth = fontRenderer->getTextLength(error, txtHeight);
        uint32_t x = (win->width - textWidth) / 2;
        uint32_t y = (win->height - txtHeight) / 2;
        fontRenderer->print(0x0, error, x, y, win->width, win->width, win->buffer, 20);
        
        widget_t* btn = win->CreateWidget(WIDGET_TYPE::Button, L"OK", 0, 100, 20, (400 - 100) / 2, 100 - 20 - 10);
        btn->Render();
    }

    void SendMessage(MSG* msg){
        msg->wc->messages[msg->wc->num_of_messages] = msg;
        msg->wc->num_of_messages++;
    }

    void Initialize()
    {
        event = nullptr;
        time = new time_t;
        wclasses = new WNDCLASS*[256];
        windows = new window_t*[100];
        fb = GetFramebuffer();
        DirEntry *FontsFolder = OpenFile('C', "FONTS      ", nullptr);
        DirEntry *fontEntry = OpenFile('C', "Roboto-Regular.ttf", FontsFolder);

        void *font = LoadFile(fontEntry);

        fontRenderer = new TTF(font);
        free(FontsFolder);
        free(fontEntry);

        sizeOfFB = fb->common.framebuffer_pitch * fb->common.framebuffer_height;
        background = RequestPages((sizeOfFB / 0x1000) + 1);

        backbuffer = RequestPages((sizeOfFB / 0x1000) + 1);

        // Load bg
        FillBuffer((void *)background, fb->common.framebuffer_width, fb->common.framebuffer_height, fb->common.framebuffer_bpp, 0x003bb0);
        memcpy(backbuffer, background, sizeOfFB);

        Refresh();

        event = (void**)malloc(sizeof(uint64_t));
        *event = nullptr;
        RegEvent(event);
        SetTimeBuff(time);

        memcpy((void *)fb->common.framebuffer_addr, backbuffer, sizeOfFB);

    }

    uint8_t min = 0;
    void LOOP(){
        while(1){
            if (*event != nullptr){
                HandleEvent();
            }
            if (min != time->minute){
                min = time->minute;
                RefreshTime();
            }
            if (mouse1Down == 1)
            {
                window_t *window = FindTopWindowAtPos(mouseX, mouseY);
                bool btn = false;

                if (window != nullptr)
                {

                    if (prevMouse1Down == 0)
                    {
                        if (window->always_on_top){
                            focus(window);
                        }
                        if (window->_Focus == 0)
                        {
                            focus(window);
                            RefreshTaskEntries();
                        }
                        else
                        {
                            for (int i = 0; i < window->num_of_widgets; i++)
                            {
                                if (1/*window->buttons[i]->clicked()*/)
                                {
                                    if (window->widgets[i] == window->closeBtn){ //Send window close msg
                                        MSG* msg = new MSG;
                                        msg->win = window;
                                        msg->code = WM_CLOSE;
                                        msg->wc = window->wc;
                                        SendMessage(msg);
                                    }else{
                                        //button clicked
                                        MSG* msg = new MSG;
                                        msg->win = window;
                                        msg->code = WM_BTN_CLICK;
                                        msg->wc = window->wc;
                                        msg->param1 = (uint64_t)window->widgets[i];
                                        //msg->param2 = window->buttons[i]->param;
                                        SendMessage(msg);
                                    }
                                    //window->buttons[i]->onclick(window, window->buttons[i]->callback_atr);
                                    btn = true;
                                }
                            }
                        }
                    }
                }

                if (btn == 0){
                    if (HandleMoveBorder(window, mouseX, mouseY, false) == -1 && window != nullptr){
                        if (prevMouse1Down == 0){
                            MSG* msg = new MSG;
                            msg->win = window;
                            msg->code = WM_MOUSE_BTN_1;
                            msg->param1 = mouseX - window->x;
                            msg->param2 = mouseY - window->y;
                            msg->wc = window->wc;
                            SendMessage(msg);
                        }
                    }
                }
            }else{
                HandleMoveBorder(nullptr, 0, 0, true);   
            }

            UpdateScreen();
            if (mouseUpdated){
                DrawMouse();
                mouseUpdated = 0;
            }
            memcpy((void*)fb->common.framebuffer_addr, backbuffer, sizeOfFB); // Slow but will work
        }
    }
}