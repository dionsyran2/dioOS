#pragma once
#include <stdint.h>
#include "memory.h"

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
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t size);
const char* charToConstCharPtr(char c);
size_t strlen(const char* string);
void strSplit(const char* str, char delimiter, int* length, char into[4096][4096]);
void strSplit(const char* str, char delimiter, int* length, char into[4096][4096], int b);
void strSplit(const char* str, char delimiter, int* length, char into[20][1024], int b);
void strSplit(const char* str, char delimiter, int* length, char**, int b);
void strcpy(char* dest, char* src);

void toUpper(char* chr, char** target);