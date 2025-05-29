#pragma once
#include "../cstr.h"
#include "../drivers/storage/volume/volume.h"

enum REG_KEY_TYPE {
    REG_STRING = 0,
    REG_BYTE   = 1,
    REG_WORD   = 2,
    REG_DWORD  = 3,
    REG_BOOL   = 4
};

struct RegKey {
    char* name;
    char* path;
    REG_KEY_TYPE type;
    union {
        char* str_value;
        uint8_t byte_value;
        uint16_t word_value;
        uint32_t dword_value;
        bool bool_value;
    };
};

void InitRegistry();
void RegQueryValue(char* keyPath, char* valueName, void* data);