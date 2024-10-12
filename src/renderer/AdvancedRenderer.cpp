#include "AdvancedRenderer.h"
#include "../memory.h"
#include "../cstr.h"
#include "../memory/heap.h"

bool MouseDrawnA = false;
uint64_t MouseLayer[1080][1920] = {{0}};
AdvancedRenderer::AdvancedRenderer(Framebuffer* targetFramebuffer_, PSF1_FONT* PSF1_Font_){
    targetFramebuffer = targetFramebuffer_;
    PSF1_Font = PSF1_Font_;
}

void AdvancedRenderer::PutPixA(uint32_t X, uint32_t Y, uint32_t Colour){
    *(uint32_t*)((uint64_t)targetFramebuffer->BaseAddress + (X*4) + (Y * targetFramebuffer->PixelsPerScanLine * 4)) = Colour;
}

uint32_t AdvancedRenderer::GetPixA(uint32_t X, uint32_t Y){
    return *(uint32_t*)((uint64_t)targetFramebuffer->BaseAddress + (X*4) + (Y * targetFramebuffer->PixelsPerScanLine * 4));
}


void AdvancedRenderer::ClearMouseCursorA(uint8_t* MouseCursor, Point Position){
    if (!MouseDrawnA) return;

    int XMax = 16;
    int YMax = 16;
    int DifferenceX = targetFramebuffer->Width - Position.X;
    int DifferenceY = targetFramebuffer->Height - Position.Y;

    if (DifferenceX < 16) XMax = DifferenceX;
    if (DifferenceY < 16) YMax = DifferenceY;

    for (int Y = 0; Y < YMax; Y++){
        for (int X = 0; X < XMax; X++){
            int Bit = Y * 16 + X;
            int Byte = Bit / 8;
            if ((MouseCursor[Byte] & (0b10000000 >> (X % 8))))
            {
                PutPixA(Position.X + X, Position.Y + Y, MouseCursorBuffer[X + Y * 16]);
            }
        }
    }
}
void AdvancedRenderer::DrawOverlayMouseCursorA(uint8_t* MouseCursor, Point Position, uint32_t Colour){
    int XMax = 16;
    int YMax = 16;
    int DifferenceX = targetFramebuffer->Width - Position.X;
    int DifferenceY = targetFramebuffer->Height - Position.Y;

    if (DifferenceX < 16) XMax = DifferenceX;
    if (DifferenceY < 16) YMax = DifferenceY;

    for (int Y = 0; Y < YMax; Y++){
        for (int X = 0; X < XMax; X++){
            int Bit = Y * 16 + X;
            int Byte = Bit / 8;
            if ((MouseCursor[Byte] & (0b10000000 >> (X % 8))))
            {
                MouseCursorBuffer[X + Y * 16] = GetPixA(Position.X + X, Position.Y + Y);
                PutPixA(Position.X + X, Position.Y + Y, Colour);
                MouseCursorBufferAfter[X + Y * 16] = GetPixA(Position.X + X, Position.Y + Y);

            }
        }
    }

    MouseDrawnA = true;
}



Layer* AdvancedRenderer::CreateLayer(uint16_t index){
    Layer* layer;
    Layers[index] = layer;
    Layers[index]->framebuffer = targetFramebuffer;
    Layers[index]->LayerPix = new uint64_t[(targetFramebuffer->Height * targetFramebuffer->Width)+targetFramebuffer->Width]();
    LayerNumber++;
    return Layers[index];
}
//x + (y * targetFramebuffer->PixelsPerScanLine)
void Layer::LDrawPixels(unsigned int xStart, unsigned int xEnd, unsigned int yStart, unsigned int yEnd, uint64_t color) {
    for (unsigned int y = yStart; y < yEnd; y++) {
        for (unsigned int x = xStart; x < xEnd; x++) {
            LayerPix[(y * framebuffer->Width) + x] = color;
        }
    }
}

void Layer::Clr(){
    for (unsigned int y = 0; y < framebuffer->Height; y++) {
        for (unsigned int x = 0; x < framebuffer->Width; x++) {
            LayerPix[(y * framebuffer->Width) + x] = 0;
        }
    }
}

uint64_t* AdvancedRenderer::CombineLayers(){
    uint64_t* combination = new uint64_t[(targetFramebuffer->Height *targetFramebuffer->Width)+targetFramebuffer->Width]();
    for (int i = 0; i < LayerNumber; i++){
        for (int y = 0; y < targetFramebuffer->Height-1; y++){
            for (int x = 0; x < targetFramebuffer->Width-1; x++){   
                uint64_t entry = Layers[i]->LayerPix[(y * targetFramebuffer->Width) + x];
                if (!entry) entry = 0;
                combination[(y * targetFramebuffer->Width) + x] = entry;
            }
        }
    }
    return combination;
}

bool upd = false;
void AdvancedRenderer::Update(){
    if (upd) return;
    upd = true;
    uint64_t* pixInfo = CombineLayers();
    for (int y = 0; y < targetFramebuffer->Height -1; y++){
        for (int x = 0; x < targetFramebuffer->Width -1; x++){
            uint64_t entry = pixInfo[(y * targetFramebuffer->Width) + x];
            globalRenderer->PutPix(x, y, entry);
        }
    }

    free(pixInfo);
    upd = false;
}