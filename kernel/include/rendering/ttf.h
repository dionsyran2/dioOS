#pragma once
#include <stdint.h>
#include <stddef.h>
#include <rendering/stb_truetype.h>

struct TTF{
    stbtt_fontinfo info;
    void* font;
    int advance_width = 0, left_side_bearing;
    int ascent, descent, line_gap;

    TTF(void* font);
    ~TTF();
    uint32_t get_text_width(wchar_t* str, uint8_t size);
    unsigned char* getBitmap(wchar_t chr, uint8_t size, int *width, int *height, int * x0, int * y0);
    void draw_char(wchar_t chr, uint32_t X, uint32_t Y, uint8_t size, uint32_t color, display* screen);
};