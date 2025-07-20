#pragma once
#include <stdint.h>

namespace QWERTYKeyboard{

    /*#define LeftShift 0x2A
    #define RightShift 0x36
    #define Enter 0x1C
    #define BackSpace 0x0E
    #define Spacebar 0x39
    #define LeftAlt 0x38
    #define LeftCtrl 0x1D
    #define KEY_DEL 0xE0
    #define SCROLL_LOCK 0x46
    #define NUM_LOCK 0x45
    #define CAPS_LOCK 0x3A*/
    extern const char ASCIITable[];
    char Translate(uint8_t scancode, bool uppercase, bool numlock);
}