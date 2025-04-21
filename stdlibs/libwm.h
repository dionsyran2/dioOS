#pragma once
#include <stddef.h>
#include <stdint.h>
#include "libc.h"

#define WS_TASKBAR (1 << 5)

#define WM_DESTROY 0xDEAD
#define WM_CLOSE 0x000C
#define WM_CREATE 0x0001
#define WM_PAINT 0x0002
#define WM_QUIT 0x0003

#define WM_KEYSTROKE 0x1
#define WM_MOUSE_BTN_1 0x2
#define WM_MOUSE_BTN_2 0x3



typedef void (*btn_callback)(void*,uint64_t);
class WNDCLASS;
typedef struct rectangle_t{
    int x;
    int y;
    uint32_t width;
    uint32_t height;
} rectangle_t;

typedef struct window_t{
    uint8_t valid:1;
    int x;
    int y;
    uint32_t height;
    uint32_t width;
    uint32_t hiddenY; 
    uint8_t borderSize;

    wchar_t* title;
    void* buffer;

    uint8_t _zIndex;
    uint8_t _Focus:1;
    uint8_t _Refresh:1;
    rectangle_t dirty_rects[256];
    uint32_t amount_of_dirty_rects;
    WNDCLASS* wc;

    bool movable;
    bool always_on_top;
    bool minimized;

    uint32_t getPix(uint32_t x, uint32_t y);
    void setPix(int x, int y, uint32_t color);
    void clear(uint32_t color);
    void Refresh(int x, int y, int width, int height);
} window_t;

typedef struct button_t {
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint8_t disabled:1;
    wchar_t* text;
    uint32_t textColor;
    uint32_t backgroundColor;
    uint32_t border; // (amount of pixels)
    uint32_t borderColor;
    uint64_t callback_atr;
    window_t* parent;
    void render();
    bool clicked();
    bool isHovering();
    btn_callback onclick;
} button_t;

struct winCreateStruct{
    uint32_t style;
    wchar_t* ClassName;
    wchar_t* title;
    uint32_t rsv;
    int x;
    int y;
    uint32_t width;
    uint32_t height;
    window_t* parent;
    void* resv;
    thread_t* thread;
};

enum CURSOR{
    Default = 1,
};

struct MSG{
    window_t* win;
    WNDCLASS* wc;
    uint32_t code;
    uint64_t param1;
    uint64_t param2;
};

struct WNDCLASS{
    int (*WndProc)(window_t* win, uint32_t msg, uint64_t param1, uint64_t param2);
    wchar_t* ClassName;
    CURSOR hCursor; //hCursor = LoadCursor(NULL, IDC_ARROW);
    uint64_t hIcon; //hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MYAPP));
    uint16_t style;
    thread_t* thread;


    MSG* messages[32];
    uint32_t num_of_messages;
    bool _exitLoopFlag = false;
};






window_t* CreateWindow(uint32_t style, wchar_t* ClassName, wchar_t* title, uint32_t rsv, int x, int y, uint32_t width, uint32_t height, window_t* parent, void* resv, thread_t* thread);

extern "C" window_t* crWinAsm(winCreateStruct* stc);
extern "C" int GetTextWidth(wchar_t* str, int size);
extern "C" void print(wchar_t* str, int size, uint32_t clr, window_t* window, int x, int y);

extern "C" time_t* GetTime();

extern "C" void RegisterClass(WNDCLASS* wc);
extern "C" void UnregisterClass(WNDCLASS* wc);

extern "C" int GetMessage_asm(MSG* msg, wchar_t* win, uint64_t rsv2, uint64_t rsv3);
int GetMessage(MSG* msg, wchar_t* win, uint64_t rsv2, uint64_t rsv3);

extern "C" void DestroyWindow(window_t* win);

extern "C" void ShowWindow(window_t* win);

extern "C" WNDCLASS* GetClass_ASM(wchar_t* cn, thread_t* thread);
WNDCLASS* GetClass(wchar_t* cn, thread_t* thread);


int DefWindowProc(window_t* win, uint32_t msg, uint64_t param1, uint64_t param2);

void TranslateMessage(MSG* msg);

void DispatchMessage(MSG* msg);

void PostQuitMessage(wchar_t* cn, int code);

void DrawLine(uint32_t* framebuffer, int width, int height, float x0, float y0, float x1, float y1, uint32_t color);

extern "C" bool exitFlag;