#pragma once
#include <stdint.h>
#include <libc.h>
#include <cstr.h>
#include <libwm.h>


void InitTaskbar();
void AddEntry(window_t* win);
void RemoveEntry(window_t* win);

extern bool wait_to_init_taskbar;
extern window_t* start_menu;