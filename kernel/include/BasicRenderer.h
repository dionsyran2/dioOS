#pragma once
#include <math.h>
#include <SimpleFont.h>
#include <stddef.h>
#include <stdint.h>
#include <cstr.h>
#include <memory/heap.h>
#include <stdarg.h>
#include <drivers/framebuffer/framebuffer.h>
#include <rendering/ttf.h>
#include <drivers/serial.h>

#define FONT_HEIGHT 16

class BasicRenderer{
    public:
    display* screen;
    uint32_t width;
    uint32_t height;
    uint8_t font_height;
    Point cursorPos;
    PSF1_FONT* font;

    BasicRenderer(display* screen, void* ttf_font);
    void draw_char(unsigned int color, wchar_t chr, unsigned int xOff, unsigned int yOff);
    void print(unsigned int color, const wchar_t* str);
    void print(unsigned int color, const char* str);
    void printfva(uint32_t color, const wchar_t* str, va_list args);
    void printfva(uint32_t color, const char* fmt, va_list args);
    uint16_t read_unicode_table(uint32_t codepoint);
    void HandleScrolling();
    void DrawBMPScaled(int dstX, int dstY, int targetW, int targetH, uint8_t* bmpData);

    bool status = false;
    void Set(bool b);
    void Clear(unsigned int color);
    

    //tty functions
    void draw_char_tty(uint32_t color, uint32_t bg, wchar_t chr, unsigned int xOff, unsigned int yOff, bool underline);
    void draw_cursor(uint32_t x, uint32_t y, uint32_t clr);
    void clear_cursor(uint32_t x, uint32_t y);
};
void kprintf(unsigned int color, const char* str, ...);
void kprintf(const char* str, ...);

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;      // Signature "BM"
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BMPFileHeader;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

void kprintf(unsigned int color, const char* str, ...);
void kprintf(const char* str, ...);
void kprintf(unsigned int color, const wchar_t* str, ...);
void kprintf(const wchar_t* str, ...);
extern BasicRenderer* globalRenderer;
