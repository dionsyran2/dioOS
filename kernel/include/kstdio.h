#pragma once
#include <drivers/serial/serial.h>

void kprintf(const char* str, ...);
void kprintfva(const char* str, va_list args);
