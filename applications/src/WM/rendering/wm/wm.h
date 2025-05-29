#pragma once
#define WM_INTERNAL

#include <stdint.h>
#include <stddef.h>
#include <libwm.h>
#include <libc.h>
#include <math.h>
#include <cstr.h>
#include <ArrayList.h>

#include "../fonts/ttf.h"
#include "window.h"

class TTF;

void InitWM();
void MainLoop();
void focus(window_internal_t* win);
window_t* CreateWindowInternal(WNDCLASS* cl, thread_t* thread, window_t* parent, uint32_t style, uint32_t width, uint32_t height, uint32_t x, uint32_t y, const wchar_t* title);
void DestroyWindowInternal(window_t* win);

extern void** event;
extern void* background;
extern void* backbuffer;
extern void* tripplebuffer;
extern time_t* time;
extern uint64_t _zIndex;

extern TTF* renderer;

extern framebuffer_t* fb;
extern ArrayList<window_internal_t*>* windows;
extern ArrayList<WNDCLASS*>* wndclasses;
