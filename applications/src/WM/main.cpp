#include "main.h"
#include "libwm.h"
int MessageBox(window_t* parent, wchar_t* text, wchar_t* title, int btns);

int WindowProc(window_t* win, uint32_t msg, uint64_t param1, uint64_t param2){
    switch (msg)
    {
        case WM_BTN_CLICK: {
            button_t* btn = (button_t*)param1;
            if ((uint64_t)btn->parent == (uint64_t)WM::taskbar){
                WM::TaskbarHandleBtn((WM::window_t*)win, (WM::button_t*)btn);
            }
            break;
        }
        case WM_CLOSE: {
            if (MessageBox(win, L"Are you sure you want to close this window?", L"Confirmation", MB_YESNO) == IDYES){
                DestroyWindow(win);
            }
            break;
        }
        case WM_DESTROY: {
            break; //We dont want it to exit the thread :)
        }
        
        default:{
            DefWindowProc(win, msg, param1, param2);
            break;
        }
    }
}

extern "C" void _main(thread_t* task)
{
    WM::Initialize();
    WNDCLASS wc = { 0 };
    wc.ClassName = L"WNDMGR";
    wc.thread = task;
    wc.WndProc = WindowProc;

    WM::addClass((WM::WNDCLASS*)&wc);

    WM::InitTaskbar(task);

    WM::window_t *win = WM::CreateWindow((WM::WNDCLASS*)&wc, task, 480, 480, 50, 50, L"Test Window 1");
    WM::window_t *win2 = WM::CreateWindow((WM::WNDCLASS*)&wc, task, 800, 600, 400, 300, L"Test Window 2");

    for (uint32_t y = win->hiddenY; y < win->height - win->borderSize; y++)
    {
        for (uint32_t x = win->borderSize; x < win->width - win->borderSize; x++)
        {
            *(uint32_t *)((uint32_t *)win->buffer + x + (y * (win->width))) = 0xf56942;
        }
    }

    for (uint32_t y = win2->hiddenY; y < win2->height - win2->borderSize; y++)
    {
        for (uint32_t x = win2->borderSize; x < win2->width - win2->borderSize; x++)
        {
            *(uint32_t *)((uint32_t *)win2->buffer + x + (y * (win2->width))) = 0xf54260;
        }
    }
    
    WM::CreateTaskbarEntry(win);
    WM::CreateTaskbarEntry(win2);

    AddInt((void*)DWM_SYS_CALLS_HANDLER, 0x7E);


    if ((uint64_t)WM::LOOP < (uint64_t)&_Start){
        CreateThread((void*)((uint64_t)&_Start + (uint64_t)WM::LOOP), task);
    }else{
        CreateThread((void*)WM::LOOP, task);
    }

    ShowWindow((window_t*)WM::taskbar);
    ShowWindow((window_t*)win);
    ShowWindow((window_t*)win2);

    MSG msg = { 0 };
    while(GetMessage(&msg, wc.ClassName, NULL, NULL) > 0){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}



int MSGWindowProc(WM::window_t* win, uint32_t msg, uint64_t param1, uint64_t param2){
    switch (msg)
    {
        case WM_BTN_CLICK: {
            WM::button_t* btn = (WM::button_t*)param1;
            win->style = btn->param;
            PostQuitMessage(L"MSGBOXHNDLR", 0);
            break;
        }
        case WM_CLOSE:{
            //win->style = IDCANCEL;
            PostQuitMessage(L"MSGBOXHNDLR", 0);
            break;
        }
    }
}


int MessageBox(window_t* parent, wchar_t* text, wchar_t* title, int btns){
    uint32_t textWidth = WM::fontRenderer->getTextLength(text, 20);
    uint32_t width = 8 + 32 + textWidth;
    uint32_t height = 22 + 8 + 74 + 32;
    uint32_t textY = (74 / 2) + 22;
    uint32_t textX = 32 / 2;
    uint32_t dividerY = 74 + 22;
    uint32_t btnOffsetY = 11 + dividerY;
    uint32_t btnHeight = 20;
    uint32_t btnWidth = 50;

    WM::WNDCLASS wc = { 0 };
    wc.ClassName = L"MSGBOXHNDLR";
    wc.thread = GetThread();
    wc.WndProc = MSGWindowProc;

    addClass((WM::WNDCLASS*)&wc);
    WM::window_t* wn = WM::CreateWindow((WM::WNDCLASS*)&wc, GetThread(), width, height, (WM::fb->common.framebuffer_width + width) / 2, (WM::fb->common.framebuffer_height + height) / 2, title);

    WM::fontRenderer->print(0, text, textX, textY, width, width, wn->buffer, 20);

    /*for (int y = 0; y < 3; y++){
        for (int x = wn->borderSize; x < width - wn->borderSize; x++){
            uint32_t color = 0;

            wn->setPix(x, y + dividerY, color);
        }
    }*/

    uint32_t amountOfButtons = 0;

    if (btns == MB_OKTRYCANCEL){
        amountOfButtons = 3;
    }else if (btns == MB_OK){
        amountOfButtons = 1;
    }else{
        amountOfButtons = 2;
    }

    uint32_t totalBtnWidth = (btnWidth + 9) * amountOfButtons;
    uint32_t btnOffsetX = (width / 2) - (totalBtnWidth / 2);

    switch (btns)
    {
        case MB_OKCANCEL:{
            WM::button_t* ok = wn->createButton(btnWidth, btnHeight, btnOffsetX, btnOffsetY, L"OK");
            ok->render();
            ok->param = IDOK;
            btnOffsetX += btnWidth + 9;
            WM::button_t* c = wn->createButton(btnWidth, btnHeight, btnOffsetX, btnOffsetY, L"Cancel");
            c->render();
            c->param = IDCANCEL;
            wn->style = IDCANCEL;
            break;
        }
        case MB_OK:{
            WM::button_t* ok = wn->createButton(btnWidth, btnHeight, btnOffsetX, btnOffsetY, L"OK");
            ok->render();
            ok->param = IDOK;
            wn->style = IDOK;
            break;
        }
        case MB_OKTRYCANCEL:{
            WM::button_t* ok = wn->createButton(btnWidth, btnHeight, btnOffsetX, btnOffsetY, L"OK");
            ok->render();
            ok->param = IDOK;
            btnOffsetX += btnWidth + 9;

            WM::button_t* tr = wn->createButton(btnWidth, btnHeight, btnOffsetX, btnOffsetY, L"Try");
            tr->render();
            tr->param = IDTRY;
            btnOffsetX += btnWidth + 9;

            WM::button_t* c = wn->createButton(btnWidth, btnHeight, btnOffsetX, btnOffsetY, L"Cancel");
            c->render();
            c->param = IDCANCEL;
            wn->style = IDCANCEL;
            break;
        }
        case MB_YESNO:{
            WM::button_t* yes = wn->createButton(btnWidth, btnHeight, btnOffsetX, btnOffsetY, L"Yes");
            yes->render();
            yes->param = IDYES;
            btnOffsetX += btnWidth + 9;

            WM::button_t* no = wn->createButton(btnWidth, btnHeight, btnOffsetX, btnOffsetY, L"No");
            no->render();
            no->param = IDNO;
            wn->style = IDNO;
            break;
        }
    }

    ShowWindow((window_t*)wn);

    MSG msg = { };
    while(GetMessage(&msg, L"MSGBOXHNDLR", NULL, NULL) > 0){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    DestroyWindow((window_t*)wn);
    UnregisterClass((WNDCLASS*)&wc);
    return wn->style;
}