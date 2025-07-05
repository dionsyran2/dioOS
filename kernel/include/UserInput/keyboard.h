#pragma once
#include <stddef.h>
#include <stdint.h>
#include <IO.h>
#include <BasicRenderer.h>
#include "ps2.h"
#include "kbScancodeTranslation.h"
#include "inputClientEvents.h"

#define PS2_KB_CMD_LED 0xED
#define PS2_KB_SCAN_CODE_SET 0xF0

// Double keyboard scan codes (0xE0 0x48)

#define CURSOR_UP_PRESSED       0x48
#define CURSOR_DOWN_PRESSED     0x50
#define CURSOR_RIGHT_PRESSED    0x4D
#define CURSOR_LEFT_PRESSED     0x4B
#define NUMPAD_ENTER            0x1C

namespace PS2KB{
    extern bool ScrollLock;
    extern bool NumberLock; 
    extern bool CapsLock;
    void SetLEDS(bool ScrollLock, bool NumberLock, bool CapsLock);
    void HandleKBInt();
    int Initialize();
}