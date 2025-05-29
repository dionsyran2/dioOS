#pragma once
#include <stdint.h>
#include <stddef.h>
#include "libc.h"

class splitString{
    public:
    uint8_t numOfStrings;
    const char* strings[];
};

const char* toString(uint64_t value);
const char* toString(int64_t value);
const char* toString(uint32_t value);
const char* toString(uint16_t value);
const char* toString(uint8_t value);
const char* toHString(uint64_t value);
const char* toHString(uint32_t value);
const char* toHString(uint16_t value);
const char* toHString(uint8_t value);
const char* toString(double value, uint8_t decimalPlaces);
const char* toString(double value);
char* strcat(char* destination, const char* source);
wchar_t* strcat(wchar_t* destination, const wchar_t* source);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t size);
int strcmp(const wchar_t* str1, const wchar_t* str2);
int strncmp(const wchar_t* str1, const wchar_t* str2, size_t size);
const char* charToConstCharPtr(char c);
size_t strlen(const char* string);
void strSplit(const char* str, char delimiter, int* length, char into[4096][4096]);
void strSplit(const char* str, char delimiter, int* length, char into[4096][4096], int b);
void strSplit(const char* str, char delimiter, int* length, char into[20][1024], int b);
void strSplit(const char* str, char delimiter, int* length, char**, int b);
void strcpy(char* dest, char* src);

void toUpper(char* chr, char** target);
wchar_t* CharToWChar(const char* input);


/* WCHAR */
const wchar_t* toWString(uint64_t value);
const wchar_t* toWHString(uint64_t value);
const wchar_t* toWHString(uint32_t value);
const wchar_t* toWHString(uint16_t value);
const wchar_t* toWHString(uint8_t value);
const wchar_t* toWString(int64_t value);
const wchar_t* toWString(double value, uint8_t decimalPlaces);
const wchar_t* toWString(double value);
const wchar_t* toWString(uint8_t value);

size_t strlen(const wchar_t* string);
void strcpy(wchar_t* dest, wchar_t* src);