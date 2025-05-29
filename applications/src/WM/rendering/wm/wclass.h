#pragma once
#include <libc.h>
#include <libwm.h>
#include "wm.h"

void RegisterClassINT(WNDCLASS* wclass);
void SendMessageINT(MSG* msg, WNDCLASS* wclass);
void UnRegisterClassINT(WNDCLASS* wclass);
WNDCLASS* FindClassINT(wchar_t* name, thread_t* thread);
