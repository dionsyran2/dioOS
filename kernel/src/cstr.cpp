#include "cstr.h"

char uinttoStringOutput[128];
const char* toString(uint64_t value){
    uint8_t size;
    uint64_t sizeTest = value;
    while (sizeTest / 10 > 0){
        sizeTest /= 10;
        size++;
    }

    uint8_t index = 0;
    while(value / 10 > 0){
        uint8_t remainder = value % 10;
        value /= 10;
        uinttoStringOutput[size - index] = remainder + '0';
        index++;
    }
    uint8_t remainder = value % 10;
    uinttoStringOutput[size - index] = remainder + '0';
    uinttoStringOutput[size + 1] = 0; 
    return uinttoStringOutput;
}

char hextoStringOutput[128];
const char* toHString(uint64_t value){
    uint64_t* valPtr = &value;
    uint8_t* ptr;
    uint8_t tmp;
    uint8_t size = 8 * 2 - 1;
    for (uint8_t i = 0; i < size; i++){
        ptr = ((uint8_t*)valPtr + i);
        tmp = ((*ptr & 0xF0) >> 4);
        hextoStringOutput[size - (i * 2 + 1)] = tmp + (tmp > 9 ? 55 : '0');
        tmp = ((*ptr & 0x0F));
        hextoStringOutput[size - (i * 2)] = tmp + (tmp > 9 ? 55 : '0');
    }
    hextoStringOutput[size + 1] = 0;
    return hextoStringOutput;
}

char hextoStringOutput32[128];
const char* toHString(uint32_t value){
    uint32_t* valPtr = &value;
    uint8_t* ptr;
    uint8_t tmp;
    uint8_t size = 4 * 2 - 1;
    for (uint8_t i = 0; i < size; i++){
        ptr = ((uint8_t*)valPtr + i);
        tmp = ((*ptr & 0xF0) >> 4);
        hextoStringOutput32[size - (i * 2 + 1)] = tmp + (tmp > 9 ? 55 : '0');
        tmp = ((*ptr & 0x0F));
        hextoStringOutput32[size - (i * 2)] = tmp + (tmp > 9 ? 55 : '0');
    }
    hextoStringOutput32[size + 1] = 0;
    return hextoStringOutput32;
}

char hextoStringOutput16[128];
const char* toHString(uint16_t value){
    uint16_t* valPtr = &value;
    uint8_t* ptr;
    uint8_t tmp;
    uint8_t size = 2 * 2 - 1;
    for (uint8_t i = 0; i < size; i++){
        ptr = ((uint8_t*)valPtr + i);
        tmp = ((*ptr & 0xF0) >> 4);
        hextoStringOutput16[size - (i * 2 + 1)] = tmp + (tmp > 9 ? 55 : '0');
        tmp = ((*ptr & 0x0F));
        hextoStringOutput16[size - (i * 2)] = tmp + (tmp > 9 ? 55 : '0');
    }
    hextoStringOutput16[size + 1] = 0;
    return hextoStringOutput16;
}

char hextoStringOutput8[128];
const char* toHString(uint8_t value){
    uint8_t* valPtr = &value;
    uint8_t* ptr;
    uint8_t tmp;
    uint8_t size = 1 * 2 - 1;
    for (uint8_t i = 0; i < size; i++){
        ptr = ((uint8_t*)valPtr + i);
        tmp = ((*ptr & 0xF0) >> 4);
        hextoStringOutput8[size - (i * 2 + 1)] = tmp + (tmp > 9 ? 55 : '0');
        tmp = ((*ptr & 0x0F));
        hextoStringOutput8[size - (i * 2)] = tmp + (tmp > 9 ? 55 : '0');
    }
    hextoStringOutput8[size + 1] = 0;
    return hextoStringOutput8;
}

char inttoStringOutput[128];
const char* toString(int64_t value){
    uint8_t isNegative = 0;

    if (value < 0){
        isNegative = 1;
        value *= -1;
        inttoStringOutput[0] = '-';
    }

    uint8_t size;
    uint64_t sizeTest = value;
    while (sizeTest / 10 > 0){
        sizeTest /= 10;
        size++;
    }

    uint8_t index = 0;
    while(value / 10 > 0){
        uint8_t remainder = value % 10;
        value /= 10;
        inttoStringOutput[isNegative + size - index] = remainder + '0';
        index++;
    }
    uint8_t remainder = value % 10;
    inttoStringOutput[isNegative + size - index] = remainder + '0';
    inttoStringOutput[isNegative + size + 1] = 0; 
    return inttoStringOutput;
}

char doubletoStringOutput[128];
const char* toString(double value, uint8_t decimalPlaces){
    if (decimalPlaces > 20) decimalPlaces = 20;

    char* intPtr = (char*)toString((int64_t)value);
    char* doublePtr = doubletoStringOutput;

    if (value < 0){
        value *= -1;
    }

    while(*intPtr != 0){
        *doublePtr = *intPtr;
        intPtr++;
        doublePtr++;
    }

    *doublePtr = '.';
    doublePtr++;

    double newValue = value - (int)value;

    for (uint8_t i = 0; i < decimalPlaces; i++){
        newValue *= 10;
        *doublePtr = (int)newValue + '0';
        newValue -= (int)newValue;
        doublePtr++;
    }

    *doublePtr = 0;
    return doubletoStringOutput;
}

const char* toString(double value){
    return toString(value, 2);
}

char* strcat(char* destination, const char* source) {
    char* ptr = destination;
    while (*ptr != '\0') {
        ptr++;
    }

    while (*source != '\0') {
        *ptr++ = *source++;
    }

    *ptr = '\0';

    return destination;
}


const char* toString(uint32_t value){
    return toString((uint64_t)value);
}
const char* toString(uint16_t value){
    return toString((uint64_t)value);
}
const char* toString(uint8_t value){
    return toString((uint64_t)value);
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) { // While characters are equal and not null
        str1++;
        str2++;
    }
    return (*str1 - *str2); // Return the difference between the first differing characters
}

#include "BasicRenderer.h"
int strncmp(const char* str1, const char* str2, size_t size) {
    int i = 0;
    while (*str1 && (*str1 == *str2) && i < size) { // While characters are equal and not null
        str1++;
        str2++;
        i++;
    }
    return (*str1 - *str2); // Return the difference between the first differing characters
}


const char* charToConstCharPtr(char c) {
    static char buffer[2]; // 1 char + 1 for null terminator
    buffer[0] = c;         // Set the character
    buffer[1] = '\0';      // Null terminate the string
    return buffer;         // Return the pointer to the buffer
}

size_t strlen(const char* string){
    char* str = (char*)string;
    size_t size = 0;
    while(*str != 0){
        str++;
        size++;
    }
    return size;
}

void strSplit(const char* str, char delimiter, int* length, char fstr[4096][4096]){
    char* chr = (char*)str;
    int i = 0;
    int len = 0;
    while (*chr != 0){
        if (*chr == delimiter){
            fstr[len][i] = '\0';
            i = 0;
            len++;
        }else{
            fstr[len][i] = *chr;
            i++;
        }
        chr++;
    }
    *length = len + 1;
    return;
}

void strSplit(const char* str, char delimiter, int* length, char fstr[4096][4096], int b){
    char* chr = (char*)str;
    int i = 0;
    int len = 0;
    int split = 1;
    while (*chr != 0){
        if (*chr == '"' && b){
            split = !split;
            chr++;
            continue;    
        };
        if (*chr == delimiter && split){
            fstr[len][i] = '\0';
            i = 0;
            len++;
        }else{
            fstr[len][i] = *chr;
            i++;
        }
        chr++;
    }
    *length = len + 1;
    return;
}

void strSplit(const char* str, char delimiter, int* length, char fstr[20][1024], int b){
    char* chr = (char*)str;
    int i = 0;
    int len = 0;
    int split = 1;
    while (*chr != 0){
        if (*chr == '"' && b){
            split = !split;
            chr++;
            continue;    
        };
        if (*chr == delimiter && split){
            fstr[len][i] = '\0';
            i = 0;
            len++;
        }else{
            fstr[len][i] = *chr;
            i++;
        }
        chr++;
    }
    *length = len + 1;
    return;
}

void strSplit(const char* str, char delimiter, int* length, char** fstr, int b){
    char* chr = (char*)str;
    int i = 0;
    int len = 0;
    int split = 1;
    while (*chr != 0){
        if (*chr == '"' && b){
            split = !split;
            chr++;
            continue;    
        };
        if (*chr == delimiter && split){
            fstr[len][i] = '\0';
            i = 0;
            len++;
        }else{
            fstr[len][i] = *chr;
            i++;
        }
        chr++;
    }
    *length = len + 1;
    return;
}


void toUpper(char* chr, char** target){
    char str[4096];
    uint8_t i = 0;
    while (*chr != 0){
        switch(*chr){
            case 'a':{
                str[i] = 'A';
                break;;
            }
            case 'b':{
                str[i] = 'B';
                break;
            }
            case 'c':{
                str[i] = 'C';
                break;
            }
            case 'd':{
                str[i] = 'D';
                break;
            }
            case 'e':{
                str[i] = 'E';
                break;
            }
            case 'f':{
                str[i] = 'F';
                break;
            }
            case 'g':{
                str[i] = 'G';
                break;
            }
            case 'h':{
                str[i] = 'H';
                break;
            }
            case 'i':{
                str[i] = 'I';
                break;
            }
            case 'j':{
                str[i] = 'J';
                break;
            }
            case 'k':{
                str[i] = 'K';
                break;
            }
            case 'l':{
                str[i] = 'L';
                break;
            }
            case 'm':{
                str[i] = 'M';
                break;
            }
            case 'n':{
                str[i] = 'N';
                break;
            }
            case 'o':{
                str[i] = 'O';
                break;
            }
            case 'p':{
                str[i] = 'P';
                break;
            }
            case 'q':{
                str[i] = 'Q';
                break;
            }
            case 'r':{
                str[i] = 'R';
                break;
            }
            case 's':{
                str[i] = 'S';
                break;
            }
            case 't':{
                str[i] = 'T';
                break;
            }
            case 'u':{
                str[i] = 'U';
                break;
            }
            case 'v':{
                str[i] = 'V';
                break;
            }
            case 'w':{
                str[i] = 'W';
                break;
            }
            case 'x':{
                str[i] = 'X';
                break;
            }
            case 'y':{
                str[i] = 'Y';
                break;
            }
            case 'z':{
                str[i] = 'Z';
                break;
            }
            default:{
                str[i] = *chr;
                break;
            }
        }
        chr++;
        i++;
    }
    str[i] = '\0';
    *target = str;
}

void strcpy(char* dest, char* src) {
    memset((void*)dest, 0, strlen(dest));
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
}