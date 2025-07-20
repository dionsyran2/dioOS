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
const char* toString(int value);
char* strcat(char* destination, const char* source);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t size);
const char* charToConstCharPtr(char c);
size_t strlen(const char* string);
void strSplit(const char* str, char delimiter, int* length, char into[4096][4096]);
void strSplit(const char* str, char delimiter, int* length, char into[4096][4096], int b);
void strSplit(const char* str, char delimiter, int* length, char into[20][1024], int b);
void strSplit(const char* str, char delimiter, int* length, char**, int b = false);
void strcpy(char* dest, char* src);

char to_lower(char c);
char to_upper(char c);

int string_to_int(char *str);
int string_to_hex(char *str);

void RemoveExcessSpaces(char* str);

char** split(const char* str, char delimiter, int& outCount);

bool isdigit(char c);

long kstrtol(const char* str, char** endptr, int base);

char* conv_wchar(wchar_t* string);