#pragma once
#include <stdint.h>
#include <stddef.h>
#include <events.h>


#define PS2_KB_CMD_SET_LED      0xED
#define PS2_KB_CMD_RESET        0xFF

#define PS2_KB_RES_ST_PASS      0xAA
#define PS2_KB_RES_ACK          0xFA
#define PS2_KB_RES_RESEND       0xFE

struct ps2_kb_led{
    union{
        uint8_t raw;
        struct {
            uint8_t scroll_lock: 1;
            uint8_t number_lock: 1;
            uint8_t caps_lock:   1;
        } bits;
    };
};

struct ps2_key_desc{
    bool state; // Pressed
    uint8_t key;
};


// Standard Set 1 Map
static const uint16_t set1_map[0xFF] = {
    0, KEY_ESC, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6,      // 0x00 - 0x07
    KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL, KEY_BACKSPACE, KEY_TAB, // 0x08 - 0x0F
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I,    // 0x10 - 0x17
    KEY_O, KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_ENTER, KEY_LEFTCTRL, KEY_A, KEY_S, // 0x18 - 0x1F
    KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, // 0x20 - 0x27
    KEY_APOSTROPHE, KEY_GRAVE, KEY_LEFTSHIFT, KEY_BACKSLASH, KEY_Z, KEY_X, KEY_C, KEY_V, // 0x28 - 0x2F
    KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_RIGHTSHIFT, KEY_KPASTERISK, // 0x30 - 0x37
    KEY_LEFTALT, KEY_SPACE, KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, // 0x38 - 0x3F
    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_NUMLOCK, KEY_SCROLLLOCK, KEY_KP7, // 0x40 - 0x47
    KEY_KP8, KEY_KP9, KEY_KPMINUS, KEY_KP4, KEY_KP5, KEY_KP6, KEY_KPPLUS, KEY_KP1, // 0x48 - 0x4F
    KEY_KP2, KEY_KP3, KEY_KP0, KEY_KPDOT, 0, 0, 0, KEY_F11,  // 0x50 - 0x57
    KEY_F12 // 0x58
};

inline static uint16_t translate_e0(uint8_t scancode) {
    switch (scancode) {
        case 0x1C: return KEY_KPENTER;
        case 0x1D: return KEY_RIGHTCTRL;
        case 0x35: return KEY_KPSLASH;
        case 0x38: return KEY_RIGHTALT;
        case 0x47: return KEY_HOME;
        case 0x48: return KEY_UP;
        case 0x49: return KEY_PAGEUP;
        case 0x4B: return KEY_LEFT;
        case 0x4D: return KEY_RIGHT;
        case 0x4F: return KEY_END;
        case 0x50: return KEY_DOWN;
        case 0x51: return KEY_PAGEDOWN;
        case 0x52: return KEY_INSERT;
        case 0x53: return KEY_DELETE;
        case 0x5B: return KEY_LEFTMETA; // Windows Key Left
        case 0x5C: return KEY_RIGHTMETA; // Windows Key Right
        case 0x5D: return KEY_COMPOSE;   // Menu Key
        default: return 0;
    }
}

namespace ps2_kb{
    extern bool kbd_ready;

    void set_lights(bool caps, bool scroll, bool num);
    void init_kb(uint8_t ident);
}