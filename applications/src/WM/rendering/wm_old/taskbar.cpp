#ifdef sdajuik

#include "wm.h"

namespace WM{
    thread_t* tsk;
    bool startDown = false;
    widget_t* startBtn = nullptr;
    widget_t* time_button = nullptr;
    window_t* startWindow;
    window_t* entries[50];
    uint8_t num_of_entries;


    void RefrestStart(){
        if (startWindow == nullptr && startDown == true){
            startWindow = CreateWindow(GetClass(L"WNDMGR", tsk), tsk, fb->common.framebuffer_width * 0.139, fb->common.framebuffer_height * 0.358, 1, fb->common.framebuffer_height - taskbar->height - (fb->common.framebuffer_height * 0.358), L"Start Menu");
            startWindow->valid = 1;
        }else if (startWindow != nullptr && startDown == false){
            RemoveWindow(startWindow);
            startWindow = nullptr;
        }
    }

    void drawTaskEntry(window_t* win, int index){
        uint32_t sx = startBtn->x + startBtn->width + 11;
        uint32_t width = fb->common.framebuffer_width * 0.12;
        uint32_t endx = taskbar->width - ((taskbar->width * 0.095) / 2) - 2;
        uint32_t sy = 4;
        uint32_t height = taskbar->height - 4 - 2;

        sx += (width + 3) * index;

        win->taskbar_btn->x = sx;
        win->taskbar_btn->y = sy;
        win->taskbar_btn->width = width;
        win->taskbar_btn->height = height;
        if (win->_Focus == 1){
            for (int y = 0; y < height; y++){
                bool off = (y % 2) != 0;

                for (int x = 0; x < width; x++){
                    taskbar->setPix(sx + x, sy + y, off ? 0xBDBDBD : 0xFFFFFF);
                    off = !off;
                }
            }
    
            for (int y = 0; y < 2; y++){
                for (int x = 0; x < width; x++){
                    uint32_t clr = 0x0;
                    if (y == 1) clr = 0x7B7B7B;
                    taskbar->setPix(sx + x, sy + y, clr);
                }
            }
    
            for (int y = 0; y < height; y++){
                for (int x = 0; x < 2; x++){
                    uint32_t clr = 0x0;
                    if (x == 1 && y != 0) clr = 0x7B7B7B;
                    taskbar->setPix(sx + x, sy + y, clr);
                }
            }
    
            for (int y = 0; y < 2; y++){
                for (int x = 0; x < width + 1; x++){
                    uint32_t clr = 0xFFFFFF;
                    
                    if (x == 0 && y == 1) continue;
                    if (y == 1) clr = 0xDEDEDE;
                    taskbar->setPix(sx + x, sy + height - y, clr);
                }
            }
    
            for (int y = 0; y < height; y++){
                for (int x = 0; x < 2; x++){
                    uint32_t clr = 0xFFFFFF;
                    if (x == 1 && y == 0) continue;
                    if (x == 1 && y != 0) clr = 0xDEDEDE;
                    taskbar->setPix(sx + width - x, sy + y, clr);
                }
            }
        }else{
            for (int y = 0; y < height; y++){
                for (int x = 0; x < width; x++){
                    taskbar->setPix(sx + x, sy + y, 0xBDBDBD);
                }
            }
    
            for (int y = 0; y < 2; y++){
                for (int x = 0; x < width; x++){
                    uint32_t clr = 0xffffff;
                    if (y == 1) clr = 0xDEDEDE;
                    taskbar->setPix(sx + x, sy + y, clr);
                }
            }
    
            for (int y = 0; y < height; y++){
                for (int x = 0; x < 2; x++){
                    uint32_t clr = 0xffffff;
                    if (x == 1 && y != 0) clr = 0xDEDEDE;
                    taskbar->setPix(sx + x, sy + y, clr);
                }
            }
    
            for (int y = 0; y < 2; y++){
                for (int x = 0; x < width + 1; x++){
                    uint32_t clr = 0x0;
                    
                    if (x == 0 && y == 1) continue;
                    if (y == 1) clr = 0x7B7B7B;
                    taskbar->setPix(sx + x, sy + height - y, clr);
                }
            }
    
            for (int y = 0; y < height; y++){
                for (int x = 0; x < 2; x++){
                    uint32_t clr = 0x0;
                    if (x == 1 && y == 0) continue;
                    if (x == 1 && y != 0) clr = 0x7B7B7B;
                    taskbar->setPix(sx + width - x, sy + y, clr);
                }
            }
        }

        fontRenderer->print(0x000000, win->title, sx + 22, 7, sx + width, taskbar->width, taskbar->buffer, height - 4);
    }
    void RefreshTaskEntries(){
        uint32_t sx = startBtn->x + startBtn->width + 11;
        uint32_t width = fb->common.framebuffer_width * 0.12;
        uint32_t endx = taskbar->width - ((taskbar->width * 0.095) / 2) - 2;
        uint32_t sy = 4;
        uint32_t height = taskbar->height - 4 - 2;

        for (int y = sy; y < sy + height; y++){
            for (int x = sx; x < endx; x++){
                taskbar->setPix(x, y, 0xC0C0C0);
            }
        }
        for (int i = 0; i < num_of_entries; i++){
            drawTaskEntry(entries[i], i);
        }
        taskbar->dirty_rects[taskbar->amount_of_dirty_rects].x = 0;
        taskbar->dirty_rects[taskbar->amount_of_dirty_rects].y = 0;
        taskbar->dirty_rects[taskbar->amount_of_dirty_rects].height = taskbar->height;
        taskbar->dirty_rects[taskbar->amount_of_dirty_rects].width = taskbar->width;
        taskbar->amount_of_dirty_rects++;
        taskbar->_Refresh = 1; 
    }

    void taskbar_entry_btn_cb(window_t* taskbar, window_t* win){
        if (win->_zIndex != taskbar->_zIndex - 1){
            win->minimized = false;
            focus(win);
            RefreshTaskEntries();
        }else{
            win->minimized = true;
            focus(taskbar);
            Refresh(win->x, win->y, win->width, win->height);
            RefreshTaskEntries();
        }
    }

    void CreateTaskbarEntry(window_t* win){
        win->taskbar_btn = taskbar->CreateWidget(WIDGET_TYPE::Button, L"", WS_VISIBLE, 0, 0, 0, 0, (uint64_t)win);
        //win->taskbar_btn->onclick = (btn_callback)taskbar_entry_btn_cb;

        entries[num_of_entries] = win;
        num_of_entries++;
        RefreshTaskEntries();
    }

    void RemoveTaskbarEntry(window_t* win){
        if (!win) return;
    
        win->CloseWindow();
    
        int index = -1;
        for (int i = 0; i < num_of_entries; i++) {
            if (entries[i] == win) {
                index = i;
                break;
            }
        }
    
        if (index == -1) return;
    
        for (int i = index; i < num_of_entries - 1; i++) {
            entries[i] = entries[i + 1];
        }

        num_of_entries--;
        entries[num_of_entries] = nullptr;

        taskbar->RemoveWidget(win->taskbar_btn);

        RefreshTaskEntries();
    }

    void time_btn_cb(){
        DirEntry* apps = OpenFile('C', "applications", nullptr);
        DirEntry* clock = OpenFile('C', "CLOCK   ELF", apps);
        if (apps == nullptr || clock == nullptr){
            ErrorPopup(L"Could not resolve path 'C:\\applications\\clock.elf'!");
        }else{
            RunELF(clock);
        }
    }

    void RefreshTime(){
        int height = taskbar->height - 5;
        int width = (taskbar->width * 0.095) / 2;
        int sy = 3;
        int sx = taskbar->width - width - 2;

        if (time_button == nullptr){
            time_button = taskbar->CreateWidget(WIDGET_TYPE::Button, L"", WS_VISIBLE, sx, sy, width, height, UINT16_MAX - 1);
            //time_button->onclick = (btn_callback)time_btn_cb;
        }

        for (int y = 0; y < height; y++){
            for (int x = 0; x < width; x++){
                taskbar->setPix(sx + x, sy + y, 0xC0C0C0);
            }
        }

        for (int x = 0; x < width; x++){
            taskbar->setPix(sx + x, sy, 0x808080);
        }

        for (int y = 0; y < height; y++){
            taskbar->setPix(sx, sy + y, 0x808080);
        }

        for (int x = 0; x < width; x++){
            taskbar->setPix(sx + x, sy + height - 1, 0xffffff);
        }

        for (int y = 0; y < height; y++){
            taskbar->setPix(sx + width - 1, sy + y, 0xffffff);
        }

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
        char* time_string = (char*)malloc(sizeof(char)* 20);
        memset(time_string, 0, 20);
        if (hour < 10) strcat(time_string, "0");
        strcat(time_string, (char*)toString(hour));
        strcat(time_string, ":");
        if (time->minute < 10) strcat(time_string, "0");
        strcat(time_string, (char*)toString(time->minute));
        strcat(time_string, "  ");
        if (pm){
            strcat(time_string, "PM");
        }else{
            strcat(time_string, "AM");
        }

        wchar_t* w_time_string = CharToWChar(time_string);
        fontRenderer->print(0, w_time_string, sx + (2 * 9), sy + ((height - 16) / 2), taskbar->width, taskbar->width, taskbar->buffer, 16);
        free(time_string);

        taskbar->dirty_rects[taskbar->amount_of_dirty_rects].x = sx;
        taskbar->dirty_rects[taskbar->amount_of_dirty_rects].y = sy;
        taskbar->dirty_rects[taskbar->amount_of_dirty_rects].width = width;
        taskbar->dirty_rects[taskbar->amount_of_dirty_rects].height = height;
        taskbar->amount_of_dirty_rects++;
        taskbar->_Refresh = 1;

        free(time_string);
        free(w_time_string);
    }

    void RedrawStartBtn(){
        if (!startDown){
            for (int y = 0; y < startBtn->height; y++){
                for (int x = 0; x < startBtn->width; x++){
                    taskbar->setPix(x + startBtn->x, y + startBtn->y, 0xB0B0B0);
                }
            }
        }else{
            for (int y = 0; y < startBtn->height; y++){
                for (int x = 0; x < startBtn->width; x++){
                    taskbar->setPix(x + startBtn->x, y + startBtn->y, 0xC0C0C0);
                }
            }
        }

        for (int y = 0; y < 2; y++){
            for (int x = 0; x < startBtn->width; x++){
                uint32_t clr = 0xFFFFFF;
                if (y != 0) clr = 0xDFDFDF;
                taskbar->setPix(startBtn->x + x, y + startBtn->y, clr);
            }
        }

        for (int y = 0; y < startBtn->height; y++){
            for (int x = 0; x < 2; x++){
                uint32_t clr = 0xFFFFFF;
                if (x != 0 && y != 0) clr = 0xDFDFDF;
                taskbar->setPix(startBtn->x + x, y + startBtn->y, clr);
            }
        }

        for (int y = 0; y < 2; y++){
            for (int x = 0; x < startBtn->width; x++){
                uint32_t clr = 0x808080;
                if (x == 0 && y == 0) continue;
                if (y != 0 && x != startBtn->width) clr = 0x0;
                taskbar->setPix(startBtn->x + x, y + startBtn->y - 1 + startBtn->height, clr);
            }
        }

        for (int y = 0; y < startBtn->height; y++){
            for (int x = 0; x < 2; x++){
                uint32_t clr = 0x808080;
                if (x != 0) clr = 0x0;
                taskbar->setPix(startBtn->x + startBtn->width + x, y + startBtn->y, clr);
            }
        }

        fontRenderer->print(0x000000, L"Start", startBtn->x + (startBtn->width / 3), 8, startBtn->width + startBtn->x, taskbar->width, taskbar->buffer, startBtn->height - 4);   
        taskbar->dirty_rects[taskbar->amount_of_dirty_rects].x = startBtn->x;
        taskbar->dirty_rects[taskbar->amount_of_dirty_rects].y = startBtn->y;
        taskbar->dirty_rects[taskbar->amount_of_dirty_rects].width = startBtn->width;
        taskbar->dirty_rects[taskbar->amount_of_dirty_rects].height = startBtn->height;
        taskbar->amount_of_dirty_rects++;

        taskbar->_Refresh = 1;
    }

    void StartCallback(window_t* window){
        startDown = !startDown;
        RefrestStart();
        RedrawStartBtn();
    }

    void InitTaskbar(thread_t* task){
        tsk = task;
        taskbar = CreateWindow(GetClass(L"WNDMGR", task), task, fb->common.framebuffer_width, fb->common.framebuffer_height * 0.029, 0, fb->common.framebuffer_height + 1 - (fb->common.framebuffer_height * 0.029), L"Taskbar");
        /* UNDO BUTTONS ETC CreateWindow DOES */
        taskbar->num_of_widgets = 0;
        taskbar->movable = false;
        taskbar->always_on_top = true;
        for (int y = 0; y < taskbar->height; y++){
            for (int x = 0; x < taskbar->width; x++){
                uint32_t clr = 0xC0C0C0;
                if (y < 2) clr = 0xFFFFFF;
                taskbar->setPix(x, y, clr);
            }
        }
        startBtn = taskbar->CreateWidget(WIDGET_TYPE::Button, L"Start", 0, 2, 4, taskbar->width * 0.042, taskbar->height - 6, UINT16_MAX - 2);
        //startBtn->onclick = (btn_callback)StartCallback;
        // dont even bother rendering it startBtn->render();
        RedrawStartBtn();
    }

    void TaskbarHandleBtn(window_t* win, widget_t* btn){
        if (btn == startBtn){
            StartCallback(win);
        }else if (btn == time_button){
            time_btn_cb();
        }else {
            taskbar_entry_btn_cb(win, (window_t*)btn->menuID);
        }
    }
}
#endif