#pragma once
#include <stdint.h>
#include <stddef.h>


extern "C" uint8_t trampoline_blob_start[];
extern "C" uint8_t trampoline_blob_end[];

extern uint64_t ap_trampoline_base;
extern uint64_t ap_trampoline_region_size;
extern int total_woken_aps;
void start_all_aps();
void ap_setup();