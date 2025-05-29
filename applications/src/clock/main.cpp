#include "main.h"

window_t* win;
uint8_t prevSec = 0;
wchar_t* tmp_buffer;
void Update(){
    int cx = win->width / 2;
    int cy = win->hiddenY + (win->height - win->hiddenY - 20) / 2;

    time_t* time = GetTime();

    if (time->second == prevSec) return;
    prevSec = time->second;
    bool pm = false;
    if (time->hour > 11){
        pm = true;
    }

    uint8_t hour = time->hour;
    if (time->hour == 0){
        hour = 12;
    }else if (time->hour > 12){
        hour -= 12;
    }

    for (int Y = win->hiddenY; Y < win->height - win->borderSize; Y++){
        for (int X = win->borderSize; X < win->width - win->borderSize; X++){
            win->setPix(X, Y, 0xC0C0C0);
        }
    }
    
    int r = (win->width - 20) / 2;
    cx = win->width / 2;
    cy = win->hiddenY + (win->height - win->hiddenY - 20) / 2;

    for (int i = 0; i < 12; i++){
        float angle = (2.0f * M_PI / 12.0f) * i;  // i = 0 to 11
        float x = cx + r * cos(angle);
        float y = cy + r * sin(angle);

        win->setPix(x, y, 0);
        win->setPix(x, y + 1, 0);
        win->setPix(x - 1, y, 0x00FFFF);
        win->setPix(x - 1, y + 1, 0x00FFFF);
    }

    float r_sec = r * 0.9f;
    float sec_angle = (2.0f * M_PI / 60.0f) * time->second - M_PI / 2.0f;
    float sec_x = cx + r_sec * cos(sec_angle);
    float sec_y = cy + r_sec * sin(sec_angle);

    float r_min = r * 0.9f * 0.8f;
    float min_angle = (2.0f * M_PI / 60.0f) * (time->minute + time->second / 60.0f) - M_PI / 2.0f;
    float min_x = cx + r_min * cos(min_angle);
    float min_y = cy + r_min * sin(min_angle);

    float r_hour = r_min * 0.8f;
    float hour_angle = (2.0f * M_PI / 12.0f) * (time->hour % 12 + time->minute / 60.0f + time->second / 3600.0f) - M_PI / 2.0f;
    float h_x = cx + r_hour * cos(hour_angle);
    float h_y = cy + r_hour * sin(hour_angle);

    DrawLine((uint32_t*)win->buffer, win->width, win->height, cx, cy, sec_x, sec_y, 0xFF0000);
    DrawLine((uint32_t*)win->buffer, win->width, win->height, cx, cy, min_x, min_y, 0x00FF00);
    DrawLine((uint32_t*)win->buffer, win->width, win->height, cx, cy, h_x, h_y, 0x0000FF);

    wchar_t* str = tmp_buffer;
    memset(str, 0, 20);
    if (hour < 10) strcat(str, L"0");
    strcat(str, toWString(hour));
    strcat(str, L":");

    if (time->minute < 10) strcat(str, L"0");
    strcat(str, toWString(time->minute));
    strcat(str, L":");

    if (time->second < 10) strcat(str, L"0");
    strcat(str, toWString(time->second));
    strcat(str, L"  ");
    strcat(str, pm ? L"PM" : L"AM");

    print(str, 16, 0x0, win, cx - (GetTextWidth(str, 16) / 2), win->height - 20);

    win->dirty_rects[win->amount_of_dirty_rects].x = win->borderSize;
    win->dirty_rects[win->amount_of_dirty_rects].y = win->hiddenY;
    win->dirty_rects[win->amount_of_dirty_rects].width = win->width - win->borderSize;
    win->dirty_rects[win->amount_of_dirty_rects].height = win->height - win->borderSize;
    win->amount_of_dirty_rects++;
    win->_Refresh = 1;
    return;
}

void loop(){
    framebuffer_t* fb = GetFramebuffer();

    while(1){
        Update();
        Sleep(200);
    }
}

int WindowProc(window_t* win, uint32_t msg, uint64_t param1, uint64_t param2);

extern "C" void _main(thread_t* thread)
{
    tmp_buffer = new wchar_t[20];
    framebuffer_t* fb = GetFramebuffer();
    uint32_t width = 200;
    uint32_t height = 250;
    int x = (fb->width - width) / 2;
    int y = ((fb->height - height) / 2) - 30;

    WNDCLASS* cl = new WNDCLASS;
    memset(cl, 0, sizeof(WNDCLASS));

    wchar_t* cn = new wchar_t[10];
    memcpy(cn, L"Clock", sizeof(wchar_t) * 6);

    cl->ClassName = cn;
    cl->WndProc = WindowProc;
    cl->thread = thread;

    RegisterClass(cl);

    win = CreateWindow(
        0,
        L"Clock",
        L"Clock",
        NULL,
        x, y, width, height,
        nullptr,
        NULL,
        thread
    );

    ShowWindow(win);

    thread_t* sub = CreateThread((void*)loop, GetThread());
    strcpy(sub->task_name, "Clock Update Loop");
    MSG* msg = new MSG;

    while(GetMessage(msg, cn, NULL, NULL) > 0){
        TranslateMessage(msg);
        DispatchMessage(msg);
    }
    
    sub->exit();
    UnregisterClass(cl);
    free(cl);
    free(cn);
    free(msg);
}


int WindowProc(window_t* win, uint32_t msg, uint64_t param1, uint64_t param2){
    switch (msg)
    {
        
    }

    return DefWindowProc(win, msg, param1, param2);
}