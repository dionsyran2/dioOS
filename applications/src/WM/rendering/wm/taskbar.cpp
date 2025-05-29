#include "taskbar.h"
#include "wm.h"
#include "window.h"
#include "rendering.h"

ArrayList<window_t*>* entries;

window_t* taskbar = nullptr;
window_t* start_menu = nullptr;
WNDCLASS* tclass = nullptr;
bool wait_to_init_taskbar = true;

int TaskbarProc(window_t* win, uint32_t msg, uint64_t param1, uint64_t param2){
    switch(msg){
        case WM_COMMAND:
            if (param1 == 0x1002){
                window_internal_t* win = (window_internal_t*)start_menu;

                win->minimized = !win->minimized;
                RedrawScreen(win->x, win->y - WIN_TITLE_BAR_HEIGHT, win->width, win->height + WIN_TITLE_BAR_HEIGHT);
            }else{
                window_internal_t* win = (window_internal_t*)entries->get(param1);
                if (win == nullptr) return 0;
                win->minimized = !win->minimized;
                RedrawScreen(win->x, win->y - WIN_TITLE_BAR_HEIGHT, win->width, win->height + WIN_TITLE_BAR_HEIGHT);
            }
    }
}

void CreateStartMenu(){
    start_menu = CreateWindow(
        WS_ALWAYS_ON_TOP | WS_NO_TASKBAR | WS_NO_TOP_BAR | WS_STATIC,
        tclass->ClassName,
        L"Start Menu",
        NULL,
        0,
        fb->height - 450,
        300,
        400,
        nullptr,
        nullptr,
        GetThread()
    );

    ShowWindow(start_menu);

    start_menu->minimized = true;
}

void RefreshStartMenu(){
    memset(start_menu->buffer, 0x808080, start_menu->width * start_menu->height * sizeof(uint32_t));

    for (int y = 0; y < start_menu->height; y++){
        start_menu->setPix(0, y, 0x0);
        start_menu->setPix(start_menu->width - 1, y, 0x0);
    }

    for (int x = 0; x < start_menu->width; x++){
        start_menu->setPix(x, 0, 0x0);
        start_menu->setPix(x, start_menu->height - 1, 0x0);
    }

    RedrawScreen(start_menu->x, start_menu->y - WIN_TITLE_BAR_HEIGHT, start_menu->width, start_menu->height + WIN_TITLE_BAR_HEIGHT);

}

void RedrawTaskbar(){
    memset(taskbar->buffer, 0x808080, taskbar->width * taskbar->height * sizeof(uint32_t));

    ((window_internal_t*)taskbar)->widgets->clear();
    

    ((window_internal_t*)taskbar)->CreateWidget(
        WIDGET_TYPE::TYPE_BUTTON,
        L"d",
        0x1002,
        5,
        5,
        taskbar->height - 10,
        taskbar->height - 10,
        0
    );

    int x = (taskbar->height - 10) + 10;
    
    for (size_t i = 0; i < entries->length(); i++){
        ((window_internal_t*)taskbar)->CreateWidget(
            WIDGET_TYPE::TYPE_BUTTON,
            entries->get(i)->title,
            i,
            x,
            5,
            100,
            taskbar->height - 10,
            0
        );
        x += 102;
    }

    taskbar->dirty_rects[taskbar->amount_of_dirty_rects].x = 0;
    taskbar->dirty_rects[taskbar->amount_of_dirty_rects].y = 0;
    taskbar->dirty_rects[taskbar->amount_of_dirty_rects].width = taskbar->width;
    taskbar->dirty_rects[taskbar->amount_of_dirty_rects].height = taskbar->height;
    taskbar->amount_of_dirty_rects++;
    taskbar->_Refresh = true;
}

void InitTaskbar(){
    
    entries = new ArrayList<window_t*>();
    tclass = new WNDCLASS;
    memset(tclass, 0, sizeof(WNDCLASS));
    tclass->thread = GetThread();
    tclass->WndProc = TaskbarProc;
    tclass->ClassName = new wchar_t[20];
    strcpy(tclass->ClassName,  L"SYS_TASKBAR");

    RegisterClass(tclass);

    
    taskbar = CreateWindow(
        WS_ALWAYS_ON_TOP | WS_NO_TASKBAR | WS_NO_TOP_BAR | WS_STATIC,
        tclass->ClassName,
        L"TASKBAR",
        NULL,
        0,
        fb->height - 50,
        fb->width,
        50,
        nullptr,
        nullptr,
        GetThread()
    );

    CreateStartMenu();
    RefreshStartMenu();

    while (wait_to_init_taskbar) Sleep(500);

    ShowWindow(taskbar);

    MSG* msg = new MSG;
    memset(msg, 0, sizeof(MSG));

    while (GetMessage(msg, L"SYS_TASKBAR", NULL, NULL) > 0)
    {
        TranslateMessage(msg);
        DispatchMessage(msg);
    }
    while(1);
    
}

void AddEntry(window_t* win){
    if (taskbar == nullptr) return;
    if (win->style & WS_NO_TASKBAR) return;
    entries->add(win);

    RedrawTaskbar();
}

void RemoveEntry(window_t* win){
    if (taskbar == nullptr) return;
    if (win->style & WS_NO_TASKBAR) return;
    entries->remove(win);

    RedrawTaskbar();
}