#include "events.h"
#include "wm.h"
#include "mouse.h"
#include "widget.h"
#include "../../keyboard/kbScancodeTranslation.h"

bool CAPS, NUMLOCK, SCROLL, SHIFT;

void ProcessKeypress(uint8_t scancode);

PIPE keyboard = -1;
PIPE mouse = -1;
void HandleEvent(){
    if (keyboard == -1) keyboard = GetPipe("\\devices\\keyboard");
    char* data = ReadPipe(keyboard);
    if (data != nullptr){
        int len = strlen(data);
    
        while(*data != NULL){
            char scancode = *data;
            data++;
            char flags = *data;
            data++;

            CAPS    = flags & (1 << 0);
            NUMLOCK = flags & (1 << 1);
            SCROLL  = flags & (1 << 2);
            SHIFT   = flags & (1 << 3);

            ProcessKeypress(scancode);
        }
    }

    EVENT_KB* kb = (EVENT_KB*)*event;
    if (kb == nullptr) return;
    *event = kb->nextEvent;
    switch(kb->type){
        case 2:{ // Mouse
            EVENT_MOUSE* mouse = (EVENT_MOUSE*)kb;
            UpdateMouse(mouse->x, mouse->y, mouse->data & 1, (mouse->data & (1 << 2)) >> 2);
            break;
        }
        default:{ 
            break;
        }
    }

    //delete[] data;
}


void ProcessKeypress(uint8_t scancode){
    if (scancode & 0x80) return; //key up
    bool caps = false;
    if (CAPS && SHIFT){
        caps = false;
    }else if (CAPS || SHIFT){
        caps = true;
    }

    wchar_t ascii = QWERTYKeyboard::Translate(scancode, caps, NUMLOCK);
    if (scancode == BackSpace) ascii = L'\b';
    if (scancode == Spacebar) ascii = L' ';
    if (ascii == 0) return; // no printable char

    if (_focused_widget != nullptr){
        _focused_widget->handleKey(ascii);
    }
    return;
}