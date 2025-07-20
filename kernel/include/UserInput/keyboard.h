#pragma once
#include <stddef.h>
#include <stdint.h>
#include <IO.h>
#include <BasicRenderer.h>
#include "ps2.h"
#include "kbScancodeTranslation.h"
#include "inputClientEvents.h"

#define PS2_KB_CMD_LED          0xED
#define PS2_KB_SCAN_CODE_SET    0xF0
#define PS2_KB_EXTENDED_BYTE    0xE0
#define KEY_UP_OFFSET           0x80

namespace PS2KB{
    enum KEY_DOWN{
    ESCAPE      = 0x1,
    ONE         = 0x2,
    TWO         = 0x3,
    THREE       = 0x4,
    FOUR        = 0x5,
    FIVE        = 0x6,
    SIX         = 0x7,
    SEVEN       = 0x8,
    EIGHT       = 0x9,
    NINE        = 0xA,
    ZERO        = 0xB,
    DASH        = 0xC, 
    // ...

    BACKSPACE   = 0xE,
    TAB         = 0xF,
    ENTER       = 0x1C,
    LEFT_CTRL   = 0x1D,
    LEFT_SHIFT  = 0x2A,
    RIGHT_SHIFT = 0x36,
    LEFT_ALT    = 0x38,
    CAPS_LOCK   = 0x3A,
    F1          = 0x3B,
    F2          = 0x3C,
    F3          = 0x3D,
    F4          = 0x3E,
    F5          = 0x3F,
    F6          = 0x40,
    F7          = 0x41,
    F8          = 0x42,
    F9          = 0x43,
    F10         = 0x44,
    NUMLOCK     = 0x45,
    SCROLL_LOCK = 0x46,
    F11         = 0x57,
    F12         = 0x58,

    // WITH EXT BYTE (0xE0)
    NUMPAD_ENTER= 0x1C,
    CURSOR_UP   = 0x48,
    CURSOR_DOWN = 0x50,
    CURSOR_RIGHT= 0x4D,
    CURSOR_LEFT = 0x4B,
    INSERT      = 0x52,
    DELETE      = 0x53,
    HOME        = 0x47,
    END         = 0x4F,
    PAGE_UP     = 0x49,
    PAGE_DOWN   = 0x51,


};

    extern bool ScrollLock;
    extern bool NumberLock; 
    extern bool CapsLock;
    void SetLEDS(bool ScrollLock, bool NumberLock, bool CapsLock);
    void HandleKBInt();
    int Initialize();
}