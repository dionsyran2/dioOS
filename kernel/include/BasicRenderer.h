/*
%h 8-bit Hex
%hh 16-bit Hex
%l 64-bit Hex

%d 32 bit Int

*/

#pragma once
#include "math.h"
#include "multiboot2.h"
#include "SimpleFont.h"
#include <stddef.h>
#include <stdint.h>
#include "cstr.h"
//#include "UserInput/mouse.h"
#include "memory/heap.h"
#include <stdarg.h>

#include "drivers/serial.h"

class BasicRenderer{
    public:
    uint32_t MouseCursorBuffer[16 * 16];
    uint32_t MouseCursorBufferAfter[16 * 16];
    BasicRenderer(struct multiboot_tag_framebuffer* targetFramebuffer, PSF1_FONT* PSF1_Font);
    Point cursorPos;
    void* backbuffer;
    struct multiboot_tag_framebuffer* targetFramebuffer;
    PSF1_FONT* PSF1_Font;
    void print(unsigned int color, const char* str);
    void printf(unsigned int color, const char* str, ...);
    void printf(const char* str, ...);
    void printfva(uint32_t color, const char* fmt, va_list args);
    void print(const char* str);
    void srand_custom(unsigned int initialSeed);
    void updateScreen();
    unsigned int rand_custom();
    unsigned int randHexColor();
    void drawChar(unsigned int color, char chr, unsigned int xOff, unsigned int yOff);
    void DrawPixels(unsigned int xStart, unsigned int xEnd, unsigned int yStart, unsigned int yEnd, unsigned int color);
    void delay(unsigned long milliseconds);
    void DrawOverlayMouseCursor(uint8_t* MouseCursor, Point Position, uint32_t Colour);
    void ClearMouseCursor(uint8_t* MouseCursor, Point Position);
    uint32_t GetPix(uint32_t X, uint32_t Y);
    void PutPix(uint32_t X, uint32_t Y, uint32_t Colour);
    void Clear(unsigned int color);
    void ClearChar(unsigned int color);
    void HandleScrolling();
    bool status = false;
    void Set(bool b);
    
    void PutPixFB(uint32_t X, uint32_t Y, uint32_t Colour);

    //tty functions
    void draw_char_tty(uint32_t color, uint32_t bg, char chr, unsigned int xOff, unsigned int yOff, bool underline);
    void draw_cursor(uint32_t x, uint32_t y, uint32_t clr);
    void clear_cursor(uint32_t x, uint32_t y);
};

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

void DrawBMPScaled(int dstX, int dstY, int targetW, int targetH, uint8_t* bmpData, void* buffer, uint64_t pitch);

extern BasicRenderer* globalRenderer;
