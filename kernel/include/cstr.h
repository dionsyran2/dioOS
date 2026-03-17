#pragma once
#include <stdint.h>
#include <stddef.h>
#include <memory.h>
#include <stdarg.h>

#ifdef tolower
    #undef tolower
#endif

#ifdef toupper
    #undef toupper
#endif

#ifdef isdigit
    #undef isdigit
#endif
char* strtok_r(char* str, const char* delim, char** saveptr);
char* strdup(const char* src);

extern "C" char* strcat(char* destination, const char* source);

extern "C" int strcmp(const char* str1, const char* str2);
extern "C" int strncmp(const char* str1, const char* str2, size_t size);

extern "C" size_t strlen(const char* string);

extern "C" void strcpy(char* dest, char* src);

extern "C" char tolower(char c);
extern "C" char toupper(char c);

extern "C" char* strncpy(char* dest, const char* src, size_t n);
extern "C" char* strncat(char* dest, const char* src, size_t n);
extern "C" const char* strstr(const char* str, const char* substring);
extern "C" char *strrchr(const char *str, int chr);
char *strchr (const char *String, int ch);

bool isdigit(char c);

long kstrtol(const char* str, char** endptr, int base);


const char* toString(uint64_t value);
const char* toString(int64_t value);

const char* toString(uint32_t value);
const char* toString(uint16_t value);
const char* toString(uint8_t value);

const char* toHString(uint64_t value);
const char* toHString(uint32_t value);
const char* toHString(uint16_t value);
const char* toHString(uint8_t value);

const char* toString(double value, uint8_t decimalPlaces = 2);

int stringf(char* buffer, size_t buffer_size, const char* format, ...);