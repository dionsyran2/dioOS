#include <cstr.h>
#include <memory.h>
#include <memory/heap.h>
#include <printf.h>

static bool is_delim(char c, const char* delim) {
    while (*delim != '\0') {
        if (c == *delim) return true;
        delim++;
    }
    return false;
}

char* strtok_r(char* str, const char* delim, char** saveptr) {
    char* token_start;

    if (str != nullptr) {
        *saveptr = str;
    }
    

    token_start = *saveptr;
    while (*token_start != '\0' && is_delim(*token_start, delim)) {
        token_start++;
    }


    if (*token_start == '\0') {
        *saveptr = token_start;
        return nullptr;
    }


    char* token_end = token_start;
    while (*token_end != '\0' && !is_delim(*token_end, delim)) {
        token_end++;
    }

    if (*token_end == '\0') {
        *saveptr = token_end;
    } else {
        *token_end = '\0';
        *saveptr = token_end + 1;
    }

    return token_start;
}

char* strdup(const char* src) {
    if (!src) return nullptr;

    size_t len = strlen(src) + 1;

    char* dst = (char*)malloc(len);
    if (!dst) return nullptr;

    memcpy(dst, src, len);

    return dst;
}


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

extern "C" char* strcat(char* destination, const char* source) {
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

extern "C" int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) { // While characters are equal and not null
        str1++;
        str2++;
    }
    return (*str1 - *str2); // Return the difference between the first differing characters
}

extern "C" int strncmp(const char* str1, const char* str2, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (str1[i] != str2[i]) {
            return (unsigned char)str1[i] - (unsigned char)str2[i];
        }
    }
    return 0; // strings match up to 'size' characters
}


extern "C" size_t strlen(const char* string){
    if (string == nullptr) return 0;
    char* str = (char*)string;
    size_t size = 0;
    while(*str != 0){
        str++;
        size++;
    }
    return size;
}


extern "C" char tolower(char c) {
    if (c >= 'A' && c <= 'Z') return c | 0x20;
    return c;
}

extern "C" char toupper(char c) {
    if (c >= 'a' && c <= 'z') return c & ~0x20;
    return c;
}

extern "C" void strcpy(char* dest, char* src) {
    memset((void*)dest, 0, strlen(dest));
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
}

extern "C" char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;

    // Copy bytes from src to dest
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    // If we hit the null terminator in src before n bytes,
    // fill the rest of dest with null bytes.
    for (; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}

extern "C" char* strncat(char* dest, const char* src, size_t n) {
    char* ptr = dest;

    // 1. Fast forward to the end of the destination string
    while (*ptr != '\0') {
        ptr++;
    }

    // 2. Append up to n characters from src
    size_t i = 0;
    while (i < n && src[i] != '\0') {
        *ptr = src[i];
        ptr++;
        i++;
    }

    // 3. Always append a null terminator at the end
    *ptr = '\0';

    return dest;
}

// Returns a pointer to the first occurrence of 'substring' in 'str', or nullptr
extern "C" const char* strstr(const char* str, const char* substring) {
    if (!*substring) return str; // Empty substring is always found

    const char* p1 = str;
    while (*p1) {
        const char* p1_begin = p1;
        const char* p2 = substring;

        while (*p1 && *p2 && *p1 == *p2) {
            p1++;
            p2++;
        }

        if (!*p2) return p1_begin; // Found match!

        p1 = p1_begin + 1;
    }

    return nullptr;
}

extern "C" char *strrchr(const char *str, int chr){
    char *c = (char*)str;

    char *last_occurance = nullptr;

    for (; *c != '\0'; c++){
        if (*c != chr) continue;

        last_occurance = c;
    }

    return last_occurance;
}


char *strchr (const char *String, int ch){
    for ( ; (*String); String++){
        if ((*String) == (char) ch){
            return ((char *) String);
        }
    }

    return (NULL);
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

bool isdigit(char c) {
    return c >= '0' && c <= '9';
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

// @brief Creates a formatted string and places it into 'buffer'
// @returns The amount of characters written (exluding the null byte)
int stringf(char* buffer, size_t buffer_size, const char* format, ...){
    va_list args;
    va_start(args, format);

    int ret = vsnprintf_(buffer, buffer_size, format, args);

    va_end(args);
    return ret;
}