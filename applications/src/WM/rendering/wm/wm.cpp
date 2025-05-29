#include "wm.h"
#include "events.h"
#include "rendering.h"
#include "mouse.h"
#include "wclass.h"
#include "taskbar.h"

// GLOBAL VARIABLES
framebuffer_t* fb;
void* background;
void* backbuffer;
void* tripplebuffer;

uint64_t _zIndex = 0;


ArrayList<window_internal_t*>* windows;
ArrayList <WNDCLASS*>* wndclasses;

TTF* renderer = nullptr;

void** event = nullptr;
time_t* time = nullptr;


size_t size_of_fb = 0;
size_t size_of_fb_in_pages = 0;

void focus(window_internal_t* win){
    for (int i = 0; i < windows->length(); i++){
        window_internal_t* w = windows->get(i);
        w->_Focus = false;
    }

    win->_Focus = true;
    win->_zIndex = _zIndex;
    _zIndex++;
    RedrawScreen(win->x, win->y, win->width, win->height);
}

// Helpers

void LoadBackground(){
    char* path;
    RegQueryValue("HKEY_LOCAL_MACHINE\\SYS_SETTINGS\\WM", "BACKGROUND", &path);

    DirEntry* entry = OpenFile("C:\\IMAGES     \\wallpaper_default.bmp");
    
    sys_log(entry->name);
    if (entry){
        uint8_t* data = (uint8_t*)LoadFile(entry);

        DrawBMPScaled(0, 0, fb->width, fb->height, data, background, fb->pitch);

        CloseFile(entry);
    }else{
        memset(background, 0x00003bb0, size_of_fb);
    }
}

// Initialization

size_t CalculateSizeOfFramebuffer(framebuffer_t* framebuffer){
    return framebuffer->pitch * framebuffer->height;
}

window_t* CreateWindowInternal(WNDCLASS* cl, thread_t* thread, window_t* parent, uint32_t style, uint32_t width, uint32_t height, uint32_t x, uint32_t y, const wchar_t* title){
    if (cl == nullptr) return nullptr;
    
    window_internal_t* window = new window_internal_t;
    memset(window, 0, sizeof(window_internal_t));

    window->wc = cl;
    window->width = width;
    window->height = height;
    window->x = x;
    window->y = y;
    window->movable = true;
    window->title = (wchar_t*)title;
    window->style = style;

    window->widgets = new ArrayList<WIDGET*>();

    size_t bytes = width * height * sizeof(uint32_t);
    size_t pages = (bytes + 0xFFF) / 0x1000;
    window->buffer = RequestPages(pages + 1);

    window->_zIndex = _zIndex;
    _zIndex++;
    windows->add(window);

    focus(window);

    AddEntry((window_t*)window);

    return (window_t*)window;
}

void DestroyWindowInternal(window_t* win){
    window_internal_t* window = (window_internal_t*)win;
    if (window == nullptr) return;
    windows->remove(window);

    RedrawScreen(window->x, window->y - WIN_TITLE_BAR_HEIGHT, window->width, window->height + WIN_TITLE_BAR_HEIGHT);

    MSG* msg = new MSG;
    msg->code = WM_DESTROY;
    msg->wc = win->wc;
    msg->param1 = (uint64_t)win->wc;
    SendMessageINT(msg, win->wc);
    RemoveEntry(win);
    free(window);
}

void EventHandlingProc(){
    while(1){
        HandleEvent();
        Sleep(30);
    }
}

uint8_t min = 0;

void MainLoop(){
    while(1){
        if (_int_moving_window){
            HandleMove(nullptr, mouseX, mouseY, false);
        }

        //if (_int_render_bb_lock) continue;
        UpdateScreen();
        memcpy((void*) fb->addr, backbuffer, size_of_fb);
        //_int_finished_copying_bb_to_fb = true;
    }
}


void InitWM(){
    _focused_widget = nullptr;
    time = new time_t;

    windows = new ArrayList<window_internal_t*>();
    wndclasses = new ArrayList<WNDCLASS*>();

    fb = GetFramebuffer();
    DirEntry *fontEntry = OpenFile("C:\\FONTS      \\Roboto-Regular.ttf");

    void *font = LoadFile(fontEntry);


    renderer = new TTF(font);

    //CloseFile(fontEntry);



    size_of_fb = CalculateSizeOfFramebuffer(fb);
    size_of_fb_in_pages = size_of_fb / 0x1000;
    background = RequestPages(size_of_fb_in_pages + 1);

    backbuffer = RequestPages(size_of_fb_in_pages + 1);

    tripplebuffer = RequestPages(size_of_fb_in_pages + 1);

    LoadBackground();
    memcpy(backbuffer, background, size_of_fb);

    event = (void**)malloc(sizeof(event));
    *event = nullptr;
    RegEvent(event);

    SetTimeBuff(time);

    memcpy((void *)fb->addr, backbuffer, size_of_fb);

    thread_t* thread = CreateThread((void*)EventHandlingProc, GetThread());
    strcpy(thread->task_name, "WM Event Handler");
}