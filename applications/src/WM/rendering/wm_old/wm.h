#pragma once
#include <stdint.h>
#include "../fonts/ttf.h"
#include "libc.h"
#include "cstr.h"
#include "icons.h"
#include "kbScancodeTranslation.h"

class TTF;

#define WS_NOBACKGROUND (1 << 0)
#define WS_DISABLED (1 << 1)
#define WS_CHILD (1 << 2)
#define WS_VISIBLE (1 << 3)
#define WS_NOBORDER (1 << 4)
#define WS_TASKBAR (1 << 5)


#define WM_DESTROY 0xDEAD
#define WM_CLOSE 0x000C
#define WM_CREATE 0x0001
#define WM_PAINT 0x0002


#define WM_KEYSTROKE 0x1
#define WM_MOUSE_BTN_1 0x2
#define WM_MOUSE_BTN_2 0x3
#define WM_BTN_CLICK   0x4

/* MB_BTN */
#define MB_YESNO 0x0001
#define MB_OKCANCEL 0x0002
#define MB_OKTRYCANCEL 0x0003
#define MB_OK 0x0004

/* IDBTN */
#define IDYES    0x0001
#define IDNO     0x0002
#define IDOK     0x0003
#define IDCANCEL 0x0004
#define IDTRY    0x0005


namespace WM{
    extern thread_t* tsk;

    typedef struct window_t window_t;
    typedef void (*btn_callback)(window_t*,uint64_t);
    class WNDCLASS;
    
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

    typedef struct{
        int x;
        int y;
        uint32_t width;
        uint32_t height;
    } rectangle_t;

    enum WIDGET_TYPE{
        Button = 0,
        TextBox = 1,
        Label = 2,
        Panel = 3,
        Slider = 10
    };

    typedef struct widget_t{
        int x, y, width, height;
        int cursor_x, cursor_y; //used for the textbox
        uint32_t style;
        WIDGET_TYPE type;
        bool focused;
        uint64_t menuID;
        wchar_t* text;
        wchar_t* placeHolder;
        uint64_t extra;
        uint8_t fontSize;
        bool _Refresh;
        window_t* parent;
        bool Clicked();
        void Render();
    } widget_t;

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
        uint64_t param;
        window_t* parent;
        void render();
        bool clicked();
        bool isHovering();
    } button_t;

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
        //button_t* createButton(uint32_t width, uint32_t height, uint32_t x, uint32_t y, wchar_t* text);
        void refreshTitleBar();
        void setName(wchar_t* name);
        void CloseWindow();
        //void removeButton(widget_t* btn);

        void RemoveWidget(widget_t* widget);
        widget_t* CreateWidget(WIDGET_TYPE type, wchar_t* text, uint32_t style, int x, int y, int width, int height, uint64_t id, uint64_t extra);
        widget_t* CreateWidget(WIDGET_TYPE type, wchar_t* text, uint32_t style, int x, int y, int width, int height, uint64_t id);
        widget_t* widgets[100];
        uint8_t num_of_widgets;
        widget_t* taskbar_btn;
        widget_t* closeBtn;
        uint32_t style;
        window_t* parent;
    } window_t;

    struct EVENT_KB{
        uint8_t type; // = 1
        uint8_t scancode;
        uint8_t data;
        uint8_t rsv[7];
        void* nextEvent;
    };

    struct EVENT_MOUSE{
        uint8_t type; // = 2
        uint32_t x;
        uint32_t y;
        uint8_t data;
        void* nextEvent;
    };

    struct EVENT_DEFAULT{
        uint8_t type;
        uint64_t data;
        uint8_t data2;
        void* nextEvent;
    };

    extern framebuffer_t* fb;
    extern size_t sizeOfFB;
    extern void* background;
    extern void* backbuffer;
    extern TTF* fontRenderer;
    extern window_t* taskbar;
    void Initialize();
    void LOOP();
    window_t *CreateWindow(WNDCLASS* cl, thread_t* thread, window_t* parent, uint32_t style, uint32_t width, uint32_t height, uint32_t x, uint32_t y, wchar_t* title);
    window_t *CreateWindow(WNDCLASS* cl, thread_t* thread, uint32_t width, uint32_t height, uint32_t x, uint32_t y, wchar_t* title);
    
    void InitTaskbar(thread_t* task);
    void RefreshTime();
    void closeWindow(window_t* window);
    void CreateTaskbarEntry(window_t* win);
    void RefreshTaskEntries();
    void RemoveTaskbarEntry(window_t* win);
    void focus(window_t*);
    void RemoveWindow(window_t* win);
    void ErrorPopup(wchar_t* msg);
    void Refresh();
    void Refresh(int xStart, int yStart, uint32_t width, uint32_t height);
    unsigned int randHexColor();
    void addClass(WNDCLASS* cl);
    WNDCLASS* GetClass(wchar_t* CN, thread_t* thread);
    void TaskbarHandleBtn(window_t* win, widget_t* btn);
    void ClearMouse();
    void DrawMouse();
    void SendMessage(MSG* msg);
    void removeClass(WNDCLASS* cl);
}

extern time_t* time;