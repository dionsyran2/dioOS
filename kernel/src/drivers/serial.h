#pragma once
#include "../IO.h"

#define COM1 0x3F8

void serialPrint(uint16_t port, const char* str);
void serialWrite(uint16_t port, char c);
void InitSerial(uint16_t port);