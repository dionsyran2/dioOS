#pragma once
#include <stdint.h>

const char* toString(uint64_t value);
const char* toString(int64_t value);
const char* toHString(uint64_t value);
const char* toHString(uint32_t value);
const char* toHString(uint16_t value);
const char* toHString(uint8_t value);
const char* toString(double value, uint8_t decimalPlaces);
const char* toString(double value);
