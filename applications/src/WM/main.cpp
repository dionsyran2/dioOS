#include "main.h"
#include "libwm.h"
#include "rendering/wm/wclass.h"
#include "rendering/wm/rendering.h"
#include "rendering/wm/taskbar.h"

extern "C" void _main(thread_t* task)
{
    InitWM();

    AddInt((void*)DWM_SYS_CALLS_HANDLER, 0x7E);

    CreateThread((void*)InitTaskbar, GetThread());


    RedrawScreen(0, 0, fb->width, fb->height);

    DirEntry* entry = OpenFile("C:\\applications\\sys_logon.elf");
    RunELF(entry);
    
    entry = OpenFile("C:\\applications\\CLOCK   ELF");
    //RunELF(entry);
    
    MainLoop();
}

/*
🧩 Next-Level Widget Ideas (building on what you have):

These are low-complexity additions that dramatically expand app capabilities:

    LABEL: read-only TEXTBOX with no border — for static text or instructions.

    LISTVIEW or FILELIST: clickable items with optional icons — great for a file explorer.

    IMAGE: render a bitmap into a widget region — ideal for splash screens, image viewer, or logos.

    PROGRESSBAR: simple widget for installation or copy operations.

    CHECKBOX / RADIOBUTTON: settings panels.

    DROPDOWN / SELECTBOX: choose between options.

    SLIDER: for volume control in audio player.

Each of these can reuse your existing event/callback system and drawing logic with minor tweaks.
*/