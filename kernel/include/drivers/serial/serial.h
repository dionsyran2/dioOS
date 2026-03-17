/* Definitions for the serial driver */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#define COM1 0x3F8 // The first COM Port
#define COM2 0x2F8 // The second COM Port
// Addresses for further ports are unrealiable and therefore not included

void InitSerial(uint16_t port);
void serialWrite(uint16_t port, char c);
void serialPrint(uint16_t port, const char* str);
void serialf(const char* str, ...);
void serialfva(const char* str, va_list args);