#pragma once
#include "../math.h"
#include "../Framebuffer.h"
#include "../SimpleFont.h"
#include <stddef.h>
#include <stdint.h>
#include "../UserInput/mouse.h"
#include "../BasicRenderer.h"

class Layer{
    public:
    Framebuffer* framebuffer;
    uint64_t* LayerPix; //[y * width + x]
    void Clr();
    void LDrawPixels(unsigned int xStart, unsigned int xEnd, unsigned int yStart, unsigned int yEnd, uint64_t color);
};

class AdvancedRenderer{
    public:
    uint64_t* CombineLayers();
    uint8_t LayerNumber = 0;
    void PutPixA(uint32_t X, uint32_t Y, uint32_t Colour);
    uint32_t GetPixA(uint32_t X, uint32_t Y);
    AdvancedRenderer(Framebuffer* targetFramebuffer, PSF1_FONT* PSF1_Font);
    Layer* CreateLayer(uint16_t index);
    void DrawOverlayMouseCursorA(uint8_t* MouseCursor, Point Position, uint32_t Colour);
    void ClearMouseCursorA(uint8_t* MouseCursor, Point Position);
    void Update();
    uint32_t MouseCursorBuffer[16 * 16];
    uint32_t MouseCursorBufferAfter[16 * 16];
    Framebuffer* targetFramebuffer;
    PSF1_FONT* PSF1_Font;
    Layer* Layers[];
};