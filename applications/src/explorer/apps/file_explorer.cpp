#include "file_explorer.h"

char* gdir = nullptr;
window_t* window = nullptr;
WNDCLASS* wcl = nullptr;
DirectoryList* listing;


wchar_t* char_to_wchar(const char* input) {
    if (!input) return nullptr;

    size_t len = strlen(input);
    wchar_t* output = new wchar_t[len + 1]; // +1 for null terminator

    for (size_t i = 0; i < len; ++i) {
        output[i] = input[i]; // Cast to handle signed chars properly
    }

    output[len] = L'\0';
    return output;
}
void RefreshWindow();

int FileExplorerProc(window_t* win, uint32_t msg, uint64_t param1, uint64_t param2){
    switch (msg){
        case WM_COMMAND:
            DirEntry entry = listing->entries[param1];
            strcat(gdir, "\\");
            strcat(gdir, entry.name);
            RefreshWindow();
    }

    return DefWindowProc(win, msg, param1, param2);
}

void CreateExplorerWindow(){
    wcl = new WNDCLASS;
    memset(wcl, 0, sizeof(WNDCLASS));

    wcl->ClassName = new wchar_t[14];
    wcl->thread = GetThread();
    wcl->WndProc = FileExplorerProc;
    strcpy(wcl->ClassName, L"FileExplorer");

    RegisterClass(wcl);

    window = CreateWindow(
        0,
        wcl->ClassName,
        L"File Explorer",
        NULL,
        50,
        50,
        800,
        600,
        nullptr,
        NULL,
        GetThread()
    );

    memset(window->buffer, 0xC0C0C0, window->width * window->height * sizeof(uint32_t));

    ShowWindow(window);
}

void RefreshWindow(){
    char* path = gdir;
    if (strlen(path) == 0) path = nullptr;

    DirEntry* dir = nullptr; 
    if (path != nullptr) {
        dir = OpenFile(path);
    }else{
        strcpy(gdir, "C:");
    }

    listing = GetDirectoryListing('C', dir);

    memset(window->buffer, 0xC0C0C0, window->width * window->height * sizeof(uint32_t));

    if (listing == nullptr || listing->numOfEntries == 0) return;

    uint32_t cx = 20;
    uint32_t cy = 30;

    for (int i = 0; i < listing->numOfEntries; i++){
        DirEntry entry = listing->entries[i];
        if ((cx + 100) > window->width) {
            cy += 100;
            cx = 20;
        }
        CreateWidgetEx(window, WIDGET_TYPE_EX::TYPE_BUTTON_EX, CharToWChar(entry.name), i, cx, cy, 100, 100, 0);
        cx += 100;
    }

}
void _explorer_main(char* dir){
    gdir = new char[1024];
    memset(gdir, 0, 1024);

    /*if (dir != nullptr){
        memcpy(gdir, dir, strlen(dir) * sizeof(char));
    }*/
    
    CreateExplorerWindow();

    RefreshWindow();

    while(1) Sleep(1000);
}