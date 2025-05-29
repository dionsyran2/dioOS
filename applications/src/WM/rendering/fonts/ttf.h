#pragma once
#include <stdarg.h>
#include "stb_truetype.h"
#include "../../main.h"

struct TTF{
    stbtt_fontinfo info;
    void* font;
    int advance_width = 0, left_side_bearing;
    int ascent, descent, line_gap;

    TTF(void* font);
    ~TTF();
    uint32_t getTextLength(wchar_t* str, uint8_t scale);
    unsigned char* getBitmap(wchar_t chr, uint8_t size, int *width, int *height, int * x0, int * y0);
    void drawChar(wchar_t chr, uint32_t x, uint32_t y, uint32_t pitch, void* buffer, uint8_t size, uint32_t color);
    void print(unsigned int color, const wchar_t* str, uint32_t x, uint32_t y, uint32_t xlim, uint32_t pitch, void* buffer, uint8_t size);
};