/*
%h 8-bit Hex
%hh 16-bit Hex
%l 32-bit Hex
%ll 64-bit Hex

%d 32 bit Int

*/





#pragma once
#include "math.h"
#include "multiboot2.h"
#include "SimpleFont.h"
#include <stddef.h>
#include <stdint.h>
#include "cstr.h"
#include <random>
//#include "UserInput/mouse.h"
#include "memory/heap.h"
#include <stdarg.h>

class BasicRenderer{
    public:
    uint32_t MouseCursorBuffer[16 * 16];
    uint32_t MouseCursorBufferAfter[16 * 16];
    BasicRenderer(struct multiboot_tag_framebuffer* targetFramebuffer, PSF1_FONT* PSF1_Font);
    Point cursorPos;
    struct multiboot_tag_framebuffer* targetFramebuffer;
    PSF1_FONT* PSF1_Font;
    void print(unsigned int color, const char* str);
    void printf(unsigned int color, const char* str, ...);
    void printf(const char* str, ...);
    void print(const char* str);
    void srand_custom(unsigned int initialSeed);
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
    bool status = true;
    void Set(bool b);
};

extern BasicRenderer* globalRenderer;