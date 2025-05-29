#include "rendering.h"
#include "wm.h"
#include "mouse.h"

void DrawBMPScaled(int dstX, int dstY, int targetW, int targetH, uint8_t* bmpData, void* buffer, uint64_t pitch) {
    BMPFileHeader* fileHeader = (BMPFileHeader*)bmpData;
    BMPInfoHeader* infoHeader = (BMPInfoHeader*)(bmpData + sizeof(BMPFileHeader));

    if (fileHeader->bfType != 0x4D42 || infoHeader->biBitCount != 24 || infoHeader->biCompression != 0)
        return; // Not a 24-bit uncompressed BMP

    int srcW = infoHeader->biWidth;
    int srcH = infoHeader->biHeight;
    uint8_t* pixelData = bmpData + fileHeader->bfOffBits;

    int rowSize = ((srcW * 3 + 3) / 4) * 4;

    for (int y = 0; y < targetH; y++) {
        for (int x = 0; x < targetW; x++) {
            int srcX = (x * srcW) / targetW;
            int srcY = (y * srcH) / targetH;

            uint8_t* pixel = pixelData + (srcH - 1 - srcY) * rowSize + srcX * 3;

            uint8_t blue  = pixel[0];
            uint8_t green = pixel[1];
            uint8_t red   = pixel[2];

            uint32_t color = 0xFF000000 | (red << 16) | (green << 8) | blue;

            if (dstX + x < targetW && dstY + y < targetH)
                *(uint32_t*)((uint32_t*)buffer + dstX + x + ((dstY + y) * (pitch / 4))) = color;
        }
    }
}

void PutPixInBB(uint32_t x, uint32_t y, uint32_t color){
    if (x < 0 || y < 0 || x > fb->width - 1 || y > fb->height - 1) return;
    *(uint32_t*)((uint32_t*)backbuffer + x + (y * (fb->pitch / (fb->bpp / 8)))) = color;
}

uint32_t GetPixInBB(uint32_t x, uint32_t y){
    return *(uint32_t*)((uint32_t*)backbuffer + x + (y * (fb->pitch / (fb->bpp / 8))));
}


void PutPixInTB(uint32_t x, uint32_t y, uint32_t color){
    *(uint32_t*)((uint32_t*)tripplebuffer + x + (y * (fb->pitch / (fb->bpp / 8)))) = color;
}

uint32_t GetPixInTB(uint32_t x, uint32_t y){
    return *(uint32_t*)((uint32_t*)tripplebuffer + x + (y * (fb->pitch / (fb->bpp / 8))));
}


void PutPixInBG(uint32_t x, uint32_t y, uint32_t color){
    *(uint32_t*)((uint32_t*)background + x + (y * (fb->pitch / (fb->bpp / 8)))) = color;
}

uint32_t GetPixInBG(uint32_t x, uint32_t y){
    return *(uint32_t*)((uint32_t*)background + x + (y * (fb->pitch / (fb->bpp / 8))));
}

void PutPixInFB(uint32_t x, uint32_t y, uint32_t color){
    *(uint32_t*)((uint32_t*)fb->addr + x + (y * (fb->pitch / (fb->bpp / 8)))) = color;
}

uint32_t GetPixInFB(uint32_t x, uint32_t y){
    return *(uint32_t*)((uint32_t*)fb->addr + x + (y * (fb->pitch / (fb->bpp / 8))));
}

window_internal_t* getTopWindow(uint32_t x, uint32_t y){
    window_internal_t *highest = nullptr;
    /*if ((x >= taskbar->x && x < (taskbar->x + taskbar->width)) && y >= taskbar->y && y < (taskbar->y + taskbar->height))
    {
        return taskbar;
    }*/
    for (uint32_t i = 0; i < windows->length(); i++)
    {
        window_internal_t* win = windows->get(i);
        if (win->visible == false || win->minimized == true) continue;
        if (((int)x >= win->x && x < (win->x + win->width)) && (int)y >= win->y && y < (win->y + win->height))
        {
            if (highest == nullptr || (win->style & WS_ALWAYS_ON_TOP))
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

bool isMouseOnXBtn(int x, int y, window_internal_t* win) {
    if (!win->_Focus || win->minimized) return false;

    int btnX = win->x + win->closeBtn.x;
    int btnY = win->y + win->closeBtn.y;
    int btnW = win->closeBtn.width;
    int btnH = win->closeBtn.height;

    return x >= btnX && x < btnX + btnW && y >= btnY && y < btnY + btnH;
}


bool isMouseOnTitlebar(int x, int y, window_internal_t* win){
    //if (win->_Focus == false) return false;
    if (win->minimized) return false;
    /* check if its a button instead */
    if (isMouseOnXBtn(x, y, win)) return false;

    if (win->x < x && (win->x + win->width) > x && (win->y - WIN_TITLE_BAR_HEIGHT - 1) < y && win->y > y){
        return true;
    }
    return false;
}

bool insideCornerFull(int x, int y, int winWidth, int winHeight, int radius) {
    float fx = (float)x + 0.5f;
    float fy = (float)y + 0.5f;

    if (x < radius && y < radius) {
        float dx = radius - fx;
        float dy = radius - fy;
        return (dx * dx + dy * dy) <= (radius * radius);
    }

    if (x >= winWidth - radius && y < radius) {
        float dx = fx - (winWidth - radius);
        float dy = radius - fy;
        return (dx * dx + dy * dy) <= (radius * radius);
    }

    if (x < radius && y >= winHeight - radius) {
        float dx = radius - fx;
        float dy = fy - (winHeight - radius);
        return (dx * dx + dy * dy) <= (radius * radius);
    }

    if (x >= winWidth - radius && y >= winHeight - radius) {
        float dx = fx - (winWidth - radius);
        float dy = fy - (winHeight - radius);
        return (dx * dx + dy * dy) <= (radius * radius);
    }

    return true;
}

bool insideCorner(int x, int y, int winWidth, int winHeight, int radius) {
    float fx = (float)x + 0.5f;
    float fy = (float)y + 0.5f;

    // Bottom-left corner
    if (x < radius && y >= winHeight - radius) {
        float dx = radius - fx;
        float dy = fy - (winHeight - radius);
        return (dx * dx + dy * dy) <= (radius * radius);
    }
    // Bottom-right corner
    if (x >= winWidth - radius && y >= winHeight - radius) {
        float dx = fx - (winWidth - radius);
        float dy = fy - (winHeight - radius);
        return (dx * dx + dy * dy) <= (radius * radius);
    }
    return true;
}

void DrawWindowTitlebar(window_internal_t* window){
    if (!window->visible || window->minimized) return;
    if (window->style & WS_NO_TOP_BAR) return;
    uint32_t tx = 15;
    uint32_t ty = 4;
    uint32_t th = 18;
    size_t tw = renderer->getTextLength(window->title, th);
    void* tb = malloc(th * tw * sizeof(uint32_t));
    memset(tb, window->_Focus ? 0xFFFFFFFF : 0x808080, th * tw * sizeof(uint32_t));
    renderer->print(
        0x0,
        window->title,
        0,
        0,
        tw + 2,
        tw,
        tb,
        th
    );

    for (int y = 1; y <= WIN_TITLE_BAR_HEIGHT; y++)
    {
        for (int x = 0; x < (int)window->width; x++)
        {
            int gx = window->x + x;
            int gy = window->y - y;
            
            if (gx < 0 || gy < 0 || gx > fb->width - 1 || gy > fb->height - 1) continue;
            window_internal_t* topWin = getTopWindow(gx, gy);
            if (topWin){
                if (topWin->_zIndex > window->_zIndex) continue;
            }
            bool _flag_continue = false;

            for (int i = 0; i < windows->length(); i++){
                window_internal_t* w = windows->get(i);
                
                if (isMouseOnTitlebar(gx, gy, w)){
                    if (w->_zIndex > window->_zIndex){
                        _flag_continue = true;
                        break;
                    }
                }
            }

            if (_flag_continue) continue;
            if (!insideCorner(x, y, window->width, WIN_TITLE_BAR_HEIGHT, WIN_BORDER_RADIUS)) continue;

            uint32_t color = window->_Focus ? 0xFFFFFFFF : 0x808080;

            if (x >= tx && x < tx + tw && (WIN_TITLE_BAR_HEIGHT - y) > ty && (WIN_TITLE_BAR_HEIGHT - y) < ty + th){
                uint32_t tx_idx = x - tx;
                uint32_t ty_idx = (WIN_TITLE_BAR_HEIGHT - y) - ty; 
                color = *((uint32_t*)tb + tx_idx + ty_idx * tw);
            }

            PutPixInBB(gx, gy, color);
        }
    }

    window->closeBtn.x = window->width - (WIN_TITLE_BAR_HEIGHT - 5);
    window->closeBtn.y = -WIN_TITLE_BAR_HEIGHT + 5;
    window->closeBtn.width = WIN_TITLE_BAR_HEIGHT - 10;
    window->closeBtn.height = WIN_TITLE_BAR_HEIGHT - 10;

    for (int y = 0; y < window->closeBtn.height; y++){
        for (int x = 0; x < window->closeBtn.width; x++){
            window_internal_t* topWin = getTopWindow(window->x + window->closeBtn.x + x, window->y + window->closeBtn.y + y);
            if (topWin){
                if (topWin->_zIndex > window->_zIndex) continue;
            }
            bool _flag_continue = false;

            if (!insideCornerFull(x, y, window->closeBtn.width, window->closeBtn.height, window->closeBtn.width / 2)) continue;

            for (int i = 0; i < windows->length(); i++){
                window_internal_t* w = windows->get(i);
                
                if (isMouseOnTitlebar(window->x + window->closeBtn.x + x, window->y + window->closeBtn.y + y, w)){
                    if (w->_zIndex > window->_zIndex){
                        _flag_continue = true;
                        break;
                    }
                }
            }

            if (_flag_continue) continue;
            PutPixInBB(window->x + window->closeBtn.x + x, window->y + window->closeBtn.y + y, 0xFF0000);
        }
    }

    free(tb);
}

void RedrawScreen(int sx, int sy, uint32_t width, uint32_t height){ //It will redraw part of the screen. WARNING: NOT UPDATE, REDRAW!
    int mx = sx + width;
    int my = sy + height;
    for (int y = sy; y < my; y++){
        for (int x = sx; x < mx; x++){
            if (x < 0 || y < 0 || x > fb->width -1 || y > fb->height - 1) continue;
            window_internal_t* window = getTopWindow(x, y);
            uint32_t clr = 0x0;
            if (window != nullptr){
                uint32_t lx = x - window->x;
                uint32_t ly = y - window->y;
                clr = window->getPix(lx, ly);
            }else{
                clr = GetPixInBG(x, y);
            }
            
            PutPixInBB(x, y, clr);
        }
    }

    for (size_t i = 0; i < windows->length(); i++){
        DrawWindowTitlebar(windows->get(i));
    }
}

void clearBorder(int X, int Y, int width, int height){
    for (int y = Y; y < Y + height; y++){ // left border
        for (int x = 0; x < 5; x++){
            int x2 = X + x;
            if (y < 0 || y > fb->height - 1) continue;
            if (x2 < 0 || x2 > fb->width - 1) continue;
            PutPixInBB(x2, y, GetPixInTB(x2, y));
        }
    }

    for (int y = Y; y < Y + height; y++){ // right border
        for (int x = 0; x < 5; x++){
            int x2 = X + width + x - 5;

            if (y < 0 || y > fb->height - 1) continue;
            if (x2 < 0 || x2 > fb->width - 1) continue;
            PutPixInBB(x2, y, GetPixInTB(x2, y));
        }
    }
    for (int y = 0; y < 5; y++){
        for (int x = X; x < X + width; x++){ // top border
            int y2 = Y + y;
            if (y2 < 0 || y2 > fb->height - 1) continue;
            if (x < 0 || x > fb->width - 1) continue;
            PutPixInBB(x, y2, GetPixInTB(x, y2));
        }
    }
    for (int y = 0; y < 5; y++){
        for (int x = X; x < X + width; x++){
            int y2 = Y + height + y;
            if (y2 < 0 || y2 > fb->height - 1) continue;
            if (x < 0 || x > fb->width - 1) continue;
            PutPixInBB(x, y2, GetPixInTB(x, y2));
        }
    }
}

void SaveBorder(int X, int Y, int width, int height){
    for (int y = Y; y < Y + height; y++){ // left border
        for (int x = 0; x < 5; x++){
            int x2 = X + x;
            if (y < 0 || y > fb->height - 1) continue;
            if (x2 < 0 || x2 > fb->width - 1) continue;
            PutPixInTB(x2, y, GetPixInBB(x2, y));
        }
    }

    for (int y = Y; y < Y + height; y++){ // right border
        for (int x = 0; x < 5; x++){
            int x2 = X + width + x - 5;

            if (y < 0 || y > fb->height - 1) continue;
            if (x2 < 0 || x2 > fb->width - 1) continue;
            PutPixInTB(x2, y, GetPixInBB(x2, y));
        }
    }
    for (int y = 0; y < 5; y++){
        for (int x = X; x < X + width; x++){ // top border
            int y2 = Y + y;
            if (y2 < 0 || y2 > fb->height - 1) continue;
            if (x < 0 || x > fb->width - 1) continue;
            PutPixInTB(x, y2, GetPixInBB(x, y2));
        }
    }
    for (int y = 0; y < 5; y++){
        for (int x = X; x < X + width; x++){
            int y2 = Y + height + y;
            if (y2 < 0 || y2 > fb->height - 1) continue;
            if (x < 0 || x > fb->width - 1) continue;
            PutPixInTB(x, y2, GetPixInBB(x, y2));
        }
    }
}

void drawBorder(int X, int Y, int width, int height){
    ClearMouse();
    SaveBorder(X, Y, width, height);
    uint32_t color1 = 0x4a4a4a;
    uint32_t color2 = 0x808080;
    bool c = false;
    for (int y = Y; y < Y + height; y++){ // left border
        for (int x = 0; x < 5; x++){
            int x2 = X + x;
            if (y < 0 || y > fb->height - 1) continue;
            if (x2 < 0 || x2 > fb->width - 1) continue;
            
            c = !c;
            if (c) continue;

            PutPixInBB(x2, y, c ? color1 : color2);
        }
        if (height % 2 == 0) c = !c;
    }

    for (int y = Y; y < Y + height; y++){ // right border
        for (int x = 0; x < 5; x++){
            int x2 = X + width + x - 5;

            if (y < 0 || y > fb->height - 1) continue;
            if (x2 < 0 || x2 > fb->width - 1) continue;

            c = !c;
            if (c) continue;

            PutPixInBB(x2, y, c ? color1 : color2);
        }
        if (height % 2 == 0) c = !c;
    }
    for (int y = 0; y < 5; y++){
        for (int x = X; x < X + width; x++){ // top border
            int y2 = Y + y;
            if (y2 < 0 || y2 > fb->height - 1) continue;
            if (x < 0 || x > fb->width - 1) continue;

            c = !c;
            if (c) continue;

            PutPixInBB(x, y2, c ? color1 : color2);
        }
        if (width % 2 == 0) c = !c;
    }
    for (int y = 0; y < 5; y++){
        for (int x = X; x < X + width; x++){
            int y2 = Y + height + y;
            if (y2 < 0 || y2 > fb->height - 1) continue;
            if (x < 0 || x > fb->width - 1) continue;

            c = !c;
            if (c) continue;

            PutPixInBB(x, y2, c ? color1 : color2);
        }
        if (width % 2 == 0) c = !c;
    }
}

bool _int_moving_window = false;
bool _int_render_bb_lock = false;
bool _int_finished_copying_bb_to_fb = false;
bool _is_first_iteration = false;

int targX = 0;
int targY = 0;
window_internal_t *targWin = nullptr;
int pmx = 0;
int pmy = 0;

int HandleMove(window_internal_t* window, int mX, int mY, bool released){
    if ((window->style & WS_STATIC) != 0) return 0;
    if (mX < 0 || mX > fb->width || mY < 0 || mY > fb->height) return 0;
    if (window != nullptr && released == false && _int_moving_window == false){
        targWin = window;
        targX = window->x;
        targY = window->y;

        pmx = mX;
        pmy = mY;
        _int_moving_window = true;
        _is_first_iteration = true;
    }else if (_int_moving_window == true && released == true){
        clearBorder(targX, targY - WIN_TITLE_BAR_HEIGHT, targWin->width, targWin->height + WIN_TITLE_BAR_HEIGHT);

        int px = targWin->x;
        int py = targWin->y;

        targWin->x = targX;
        targWin->y = targY;
        _int_moving_window = false;

        ClearMouse();

        RedrawScreen(px, py - WIN_TITLE_BAR_HEIGHT, targWin->width, targWin->height + WIN_TITLE_BAR_HEIGHT);
        RedrawScreen(targWin->x, targWin->y - WIN_TITLE_BAR_HEIGHT, targWin->width, targWin->height + WIN_TITLE_BAR_HEIGHT);
        targWin = nullptr;

        DrawMouse();

        return 0;
    }
    if (_int_moving_window == false) return 0;
    if (targWin == nullptr) return 0;

    _int_render_bb_lock = true;
    if (!_is_first_iteration){
        clearBorder(targX, targY - WIN_TITLE_BAR_HEIGHT, targWin->width, targWin->height + WIN_TITLE_BAR_HEIGHT);
    }else{
        _is_first_iteration = false;
    }

    targX += (mX - pmx);
    targY += (mY - pmy);

    pmx = mX;
    pmy = mY;

    drawBorder(targX, targY - WIN_TITLE_BAR_HEIGHT, targWin->width, targWin->height + WIN_TITLE_BAR_HEIGHT);
    _int_render_bb_lock = false;
    return -1;
}

bool isWindowPixOnTop(window_internal_t *window, uint32_t x, uint32_t y)
{
    if (getTopWindow(window->x + x, window->y + y) == window)
    {
        for (uint32_t i = 0; i < windows->length(); i++){
            window_internal_t* win = windows->get(i);
            if (win == window) continue;
            if (isMouseOnTitlebar(window->x + x, window->y + y, win) && win->_zIndex > window->_zIndex){
                return false;
            }
            
        }
        return true;
    }

    return false;
}

void UpdateWindowOnScreen(window_internal_t *window)
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
               PutPixInBB(window->x + x, window->y + y, color);
            }
        }
    }
    window->_Refresh = 0;
    window->amount_of_dirty_rects = 0;
}

void UpdateScreen()
{
    /* Check the taskbar */

    /* WINDOWS */

    for (int i = 0; i < windows->length(); i++)
    {
        window_internal_t *window = windows->get(i);
        if (window->visible == 0)
            continue;
        if (window->minimized == true)
            continue;

        if (window->_Refresh == 1)
        {
            UpdateWindowOnScreen(window);
        }
    }

    return;
}

