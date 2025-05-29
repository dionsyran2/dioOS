#include "main.h"

int WindowProc(window_t* win, uint32_t msg, uint64_t param1, uint64_t param2);

WNDCLASS* cl;

int session_id = 1;

window_t* login_win;

WIDGETEX* username_box;

WIDGETEX* password_box;


bool validate_login(){
    return true;
    if ((strcmp(GetText(username_box), L"admin") == 0) && (strcmp(GetText(password_box), L"YouShallNotPass") == 0)){
        return true;
    }
    return false;
}


void login(){
    login_win = CreateWindow(
        0,
        cl->ClassName,
        L"Login",
        50,
        50,
        150,
        400,
        150,
        nullptr,
        nullptr,
        GetThread()
    );

    memset(login_win->buffer, 0xC0C0C0, login_win->width * login_win->height * sizeof(uint32_t));

    ShowWindow(login_win);

    wchar_t* usr_str = new wchar_t[11];
    strcpy(usr_str, L"Username:");

    print(usr_str, 18, 0x0, login_win, 10, 32);
    username_box = CreateWidgetEx(login_win, TYPE_TEXTBOX_EX, nullptr, 100, 20 + GetTextWidth(usr_str, 18), 30, 256, 22, 0);

    wchar_t* psw_str = new wchar_t[11];
    strcpy(psw_str, L"Password:");

    print(psw_str, 18, 0x0, login_win, 10, 62);

    password_box = CreateWidgetEx(login_win, TYPE_TEXTBOX_EX, nullptr, 100, 20 + GetTextWidth(usr_str, 18), 60, 256, 22, 0);

    WIDGETEX* login_btn = CreateWidgetEx(login_win, TYPE_BUTTON_EX, L"login", 0x1001, (login_win->width - 210) / 2, login_win->height - 30, 100, 20, 0);

    WIDGETEX* shutdown_btn = CreateWidgetEx(login_win, TYPE_BUTTON_EX, L"Shutdown", 0x1002, ((login_win->width - 210) / 2) + 110, login_win->height - 30, 100, 20, 0);


}

extern "C" void _main(thread_t* task)
{
    cl = new WNDCLASS;
    memset(cl, 0, sizeof(WNDCLASS));
    cl->WndProc = WindowProc;
    cl->thread = task;
    cl->ClassName = new wchar_t[9];
    strcpy(cl->ClassName, L"EXPLORER");

    RegisterClass(cl);

    login();

    PIPE pipe = CreatePipe("system\\out\\win_logon");

    MSG* msg = new MSG;
    while(GetMessage(msg, cl->ClassName, NULL, NULL) > 0){
        TranslateMessage(msg);
        DispatchMessage(msg);
    }
    
    UnregisterClass(cl);
    free(cl->ClassName);
    free(cl);
    free(msg);

    DirEntry* entry = OpenFile("C:\\applications\\explorer_app.elf");
    RunELF(entry);

    wm_sys_unblock_taskbar();

    while (1){
        Sleep(100);
        char* data = ReadPipe(pipe);
        if (data == nullptr) continue;
        sys_log(data);
        free(data);
    }
}


int WindowProc(window_t* win, uint32_t msg, uint64_t param1, uint64_t param2){
    switch (msg){
        case WM_COMMAND:
            if (param1 == 0x1002){ //shutdown btn
                SetPower(0);
            }else if (param1 == 0x1001){
                if (validate_login()){
                    DestroyWindow(login_win);
                }
            }
    }
    return DefWindowProc(win, msg, param1, param2);
}