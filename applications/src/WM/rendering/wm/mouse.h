#pragma once
#include <stdint.h>
#include <stddef.h>

#include "wm.h"

extern uint32_t mouseX;
extern uint32_t mouseY;
extern uint32_t mouse1Down;
extern uint32_t mouse2Down;
extern uint32_t mouseUpdated;
extern uint32_t prevMouseX;
extern uint32_t prevMouseY;
extern uint32_t prevMouse1Down;
extern uint32_t prevMouse2Down;

void ClearMouse();
void DrawMouse();
void UpdateMouse(uint32_t mouseX, uint32_t mouseY, uint32_t mouse1Down, uint32_t mouse2Down);