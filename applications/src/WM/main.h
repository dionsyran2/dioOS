#pragma once
#include <stdint.h>
#include "libc.h"
#include "rendering/wm/wm.h"
#include "interrupts/calls/calls.h"

extern uint64_t _Start;
extern "C" void _main(thread_t* task);