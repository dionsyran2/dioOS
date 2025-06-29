#pragma once
#include <stddef.h>
#include <stdint.h>
#include <IO.h>
#include <BasicRenderer.h>
#include "ps2.h"
#include <kernel.h>
#include "kbScancodeTranslation.h"
#include "inputClientEvents.h"

#define PS2_KB_CMD_LED 0xED
#define PS2_KB_SCAN_CODE_SET 0xF0
namespace PS2KB{
    extern bool ScrollLock;
    extern bool NumberLock; 
    extern bool CapsLock;
    void SetLEDS(bool ScrollLock, bool NumberLock, bool CapsLock);
    void HandleKBInt();
    int Initialize();
}