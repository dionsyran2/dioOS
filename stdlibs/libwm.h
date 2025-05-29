#pragma once
#include <stddef.h>
#include <stdint.h>
#include "libc.h"

#define WS_VISIBLE (1 << 0)
#define WS_DISABLED (1 << 1)
#define WS_RESIZABLE (1 << 2)
#define WS_NO_TASKBAR (1 << 5)
#define WS_ALWAYS_ON_TOP (1 << 6)
#define WS_NO_TOP_BAR (1 << 6)
#define WS_STATIC (1 << 7)


#define WM_DESTROY 0xDEAD
#define WM_CLOSE 0x000C
#define WM_CREATE 0x0001
#define WM_PAINT 0x0002
#define WM_QUIT 0x0003
#define WM_COMMAND 0x0004

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

struct BOX{
    int x;
    int y;
    uint32_t width;
    uint32_t height;
};

typedef struct window_t{
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

    uint32_t getPix(uint32_t x, uint32_t y);
    void setPix(int x, int y, uint32_t color);
    void clear(uint32_t color);
    void Refresh(int x, int y, int width, int height);
} window_t;

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


enum WIDGET_TYPE_EX{
    TYPE_BUTTON_EX,
    TYPE_TEXTBOX_EX
};

/**************/
/* BASE CLASS */
/**************/

class WIDGETEX{
    public:
    WIDGET_TYPE_EX type;

    int x, y;
    uint32_t width, height, style;
    window_t* parent;
};


/****************/
/* BUTTON CLASS */
/****************/

class BUTTONEX : public WIDGETEX{
    public:
    int id;
    const wchar_t* text; 
};


/*****************/
/* TEXTBOX CLASS */
/*****************/

class TEXTBOXEX : public WIDGETEX{
    public:
    wchar_t* text;
    size_t maxLength;
    size_t cursorPosition = 0;
    bool focused;
};



struct CreateWidgetStruct{
    CreateWidgetStruct(window_t* parent, WIDGET_TYPE_EX type, const wchar_t* text, int identifier, int x, int y, uint32_t width, uint32_t height, int style);
    window_t* parent;
    WIDGET_TYPE_EX type;
    const wchar_t* text;
    int identifier, x, y;
    uint32_t width, height;
    int style;
};




window_t* CreateWindow(uint32_t style, wchar_t* ClassName, wchar_t* title, uint32_t rsv, int x, int y, uint32_t width, uint32_t height, window_t* parent, void* resv, thread_t* thread);

extern "C" window_t* crWinAsm(winCreateStruct* stc);
extern "C" int GetTextWidth(wchar_t* str, int size);
extern "C" void print(wchar_t* str, int size, uint32_t clr, window_t* window, int x, int y);

extern "C" time_t* GetTime();

extern "C" void RegisterClass(WNDCLASS* wc);
extern "C" void UnregisterClass(WNDCLASS* wc);

extern "C" int GetMessage_asm(MSG* msg, wchar_t* win, uint64_t rsv2, uint64_t rsv3);
int GetMessage(MSG* msg, wchar_t* ClassName, uint64_t rsv2, uint64_t rsv3);

extern "C" void DestroyWindow(window_t* win);

extern "C" void ShowWindow(window_t* win);

extern "C" WNDCLASS* GetClass_ASM(wchar_t* cn, thread_t* thread);

extern "C" WIDGETEX* CreateWidgetAsm(CreateWidgetStruct* strct);

WNDCLASS* GetClass(wchar_t* cn, thread_t* thread);

WIDGETEX* CreateWidgetEx(window_t* parent, WIDGET_TYPE_EX type, const wchar_t* text, int identifier, int x, int y, uint32_t width, uint32_t height, int style);

extern "C" void RedrawWidget(WIDGETEX* widget);

int DefWindowProc(window_t* win, uint32_t msg, uint64_t param1, uint64_t param2);

void TranslateMessage(MSG* msg);

void DispatchMessage(MSG* msg);

void PostQuitMessage(WNDCLASS* cl, int code);

void DrawLine(uint32_t* framebuffer, int width, int height, float x0, float y0, float x1, float y1, uint32_t color);
void DrawCircle(uint8_t* buffer, int pitch, int centerX, int centerY, int radius, uint32_t color, bool fill);

extern "C" wchar_t* GetText(WIDGETEX* widget);
extern "C" bool exitFlag;

extern "C" void wm_sys_unblock_taskbar();