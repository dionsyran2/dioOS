#pragma once
#include <stddef.h>
#include <stdint.h>
#include <IO.h>
#include <BasicRenderer.h>
#include "ps2.h"

#define PS2Leftbutton 0b00000001
#define PS2Middlebutton 0b00000100
#define PS2Rightbutton 0b00000010
#define PS2XSign 0b00010000
#define PS2YSign 0b00100000
#define PS2XOverflow 0b01000000
#define PS2YOverflow 0b10000000


namespace PS2Mouse{
    int Initialize();
    void HandlePS2Mouse();
}