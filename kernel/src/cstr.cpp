#include <cstr.h>
#include <memory.h>
#include <memory/heap.h>

char uinttoStringOutput[128];
const char* toString(uint64_t value){
    uint8_t size = 0;
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

    uint8_t size = 0;
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

int strncmp(const char* str1, const char* str2, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (str1[i] != str2[i]) {
            return (unsigned char)str1[i] - (unsigned char)str2[i];
        }
    }
    return 0; // strings match up to 'size' characters
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


char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c | 0x20;
    return c;
}

char to_upper(char c) {
    if (c >= 'a' && c <= 'z') return c & ~0x20;
    return c;
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

int string_to_int(char *str) {
    int result = 0;
    int sign = 1;
    
    if (*str == '-') {
        sign = -1;
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return result * sign;
}

int string_to_hex(char *str) {
    int result = 0;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;  // Skip "0x" prefix
    }
    
    while ((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'f') || (*str >= 'A' && *str <= 'F')) {
        result *= 16;
        if (*str >= '0' && *str <= '9') {
            result += *str - '0';
        } else if (*str >= 'a' && *str <= 'f') {
            result += *str - 'a' + 10;
        } else if (*str >= 'A' && *str <= 'F') {
            result += *str - 'A' + 10;
        }
        str++;
    }

    return result;
}

void RemoveExcessSpaces(char* str) {
    int read = 0, write = 0;
    bool inSpace = false;

    // Skip leading spaces
    while (str[read] == ' ') read++;

    while (str[read]) {
        if (str[read] != ' ') {
            str[write++] = str[read++];
            inSpace = false;
        } else {
            if (!inSpace) {
                str[write++] = ' ';
                inSpace = true;
            }
            read++;
        }
    }

    if (write > 0 && str[write - 1] == ' ') {
        write--;
    }

    str[write] = '\0';
}

char** split(const char* str, char delimiter, int& outCount) {
    int count = 0;
    const char* p = str;

    // Count how many tokens we will have
    while (*p) {
        while (*p == delimiter) p++; // skip delimiters
        if (*p) {
            count++;
            while (*p && *p != delimiter) p++; // skip non-delimiters
        }
    }

    char** result = (char**)malloc(sizeof(char*) * count);
    p = str;
    int i = 0;

    while (*p && i < count) {
        while (*p == delimiter) p++;
        const char* start = p;

        while (*p && *p != delimiter) p++;
        int len = p - start;

        result[i] = (char*)malloc(len + 1);
        memcpy(result[i], start, len * sizeof(char));
        result[i][len] = '\0';
        i++;
    }

    outCount = count;
    return result;
}


bool is_lower(char c) {
    return c >= 'a' && c <= 'z';
}

bool is_upper(char c) {
    return c >= 'A' && c <= 'Z';
}

int char_to_digit(char c) {
    if (isdigit(c)) return c - '0';
    if (is_upper(c)) return c - 'A' + 10;
    if (is_lower(c)) return c - 'a' + 10;
    return -1;
}

long kstrtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    long result = 0;
    int sign = 1;

    // Skip whitespace
    while (*s == ' ' || *s == '\t') s++;

    // Handle sign
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    // Auto-detect base if 0
    if (base == 0) {
        if (s[0] == '0') {
            if (s[1] == 'x' || s[1] == 'X') {
                base = 16;
                s += 2;
            } else {
                base = 8;
                s++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    // Parse digits
    while (*s) {
        int digit = char_to_digit(*s);
        if (digit < 0 || digit >= base) break;

        result = result * base + digit;
        s++;
    }

    if (endptr) *endptr = (char *)s;
    return result * sign;
}

bool isdigit(char c) {
    return c >= '0' && c <= '9';
}

size_t strlen(const wchar_t* string){
    wchar_t* str = (wchar_t*)string;
    size_t size = 0;
    while(*str != 0){
        str++;
        size++;
    }
    return size;
}

char cstr[256];
char* conv_wchar(wchar_t* string){
    int offset = 0;
    wchar_t* str = (wchar_t*)string;


    while (*str != 0){
        cstr[offset] = (char)*str;
        offset++;
        str++;
    }
    return cstr;
}