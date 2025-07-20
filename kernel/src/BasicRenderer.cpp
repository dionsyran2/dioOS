#include <BasicRenderer.h>
#include <paging/PageFrameAllocator.h>
#include <scheduling/lock/spinlock.h>

BasicRenderer* globalRenderer;
spinlock_t renderer_spinlock;

void BasicRenderer::Set(bool val){
    status = val;
}

BasicRenderer::BasicRenderer(multiboot_tag_framebuffer* targetFrameBuffer, PSF1_FONT* psf1_font)
{
    targetFramebuffer = targetFrameBuffer;
    backbuffer = GlobalAllocator.RequestPages((targetFrameBuffer->common.framebuffer_height * targetFrameBuffer->common.framebuffer_pitch) / 0x1000);
    memset(backbuffer, 0, targetFrameBuffer->common.framebuffer_height * targetFrameBuffer->common.framebuffer_pitch);
    PSF1_Font = psf1_font;
    cursorPos.X = 0;
    cursorPos.Y = 0;
    cursorPos.XLim = targetFrameBuffer->common.framebuffer_width-1;
    cursorPos.overX = 0;
};

static uint32_t clr = 0;

void BasicRenderer::updateScreen(){
    memcpy_simd((void*)targetFramebuffer->common.framebuffer_addr, backbuffer, targetFramebuffer->common.framebuffer_height * targetFramebuffer->common.framebuffer_pitch);
}

void BasicRenderer::drawChar(unsigned int color, char chr, unsigned int xOff, unsigned int yOff){
    if (!status) return;
    serialWrite(COM1, chr);
    char* PSF1_FontPtr = (char*)PSF1_Font->glyphBuffer + (chr * PSF1_Font->psf1_Header->charsize);
    for (unsigned long y=yOff; y<yOff + 16; y++){
        for(unsigned long x = xOff; x<xOff + 8; x++){
            if((*PSF1_FontPtr & (0b10000000 >> (x - xOff))) > 0){
                if (x < 0 || x > targetFramebuffer->common.framebuffer_width - 1 || y < 0 || y > targetFramebuffer->common.framebuffer_height - 1) continue;
                PutPix(x, y, color);
            }
        }
        PSF1_FontPtr++;
    }
}

void BasicRenderer::draw_char_tty(uint32_t color, uint32_t bg, char chr, unsigned int xOff, unsigned int yOff, bool underline){
    char* PSF1_FontPtr = (char*)PSF1_Font->glyphBuffer + (chr * PSF1_Font->psf1_Header->charsize);
    for (unsigned long y=yOff; y<yOff + 16; y++){
        for(unsigned long x = xOff; x<xOff + 8; x++){
            if((*PSF1_FontPtr & (0b10000000 >> (x - xOff))) > 0){
                if (x < 0 || x > targetFramebuffer->common.framebuffer_width - 1 || y < 0 || y > targetFramebuffer->common.framebuffer_height - 1) continue;
                PutPixFB(x, y, color);
            }else{
                PutPixFB(x, y, bg);
            }
        }
        PSF1_FontPtr++;
    }

    if (!underline) return;
    // UNDERLINE

    PSF1_FontPtr = (char*)PSF1_Font->glyphBuffer + ('_' * PSF1_Font->psf1_Header->charsize);
    for (unsigned long y=yOff; y<yOff + 16; y++){
        for(unsigned long x = xOff; x<xOff + 8; x++){
            if((*PSF1_FontPtr & (0b10000000 >> 4)) > 0){
                if (x < 0 || x > targetFramebuffer->common.framebuffer_width - 1 || y < 0 || y > targetFramebuffer->common.framebuffer_height - 1) continue;
                PutPixFB(x, y, color);
            }
        }
        PSF1_FontPtr++;
    }
}

void BasicRenderer::draw_cursor(uint32_t xOff, uint32_t yOff, uint32_t clr){
    char* PSF1_FontPtr = (char*)PSF1_Font->glyphBuffer + ('_' * PSF1_Font->psf1_Header->charsize);
    for (unsigned long y=yOff; y<yOff + 16; y++){
        for(unsigned long x = xOff; x<xOff + 8; x++){
            if((*PSF1_FontPtr & (0b10000000 >> (x - xOff))) > 0){
                if (x < 0 || x > targetFramebuffer->common.framebuffer_width - 1 || y < 0 || y > targetFramebuffer->common.framebuffer_height - 1) continue;
                PutPixFB(x, y, clr);
            }
        }
        PSF1_FontPtr++;
    }
}
void BasicRenderer::clear_cursor(uint32_t xOff, uint32_t yOff){
    char* PSF1_FontPtr = (char*)PSF1_Font->glyphBuffer + ('_' * PSF1_Font->psf1_Header->charsize);
    for (unsigned long y=yOff; y<yOff + 16; y++){
        for(unsigned long x = xOff; x<xOff + 8; x++){
            if((*PSF1_FontPtr & (0b10000000 >> (x - xOff))) > 0){
                if (x < 0 || x > targetFramebuffer->common.framebuffer_width - 1 || y < 0 || y > targetFramebuffer->common.framebuffer_height - 1) continue;
                PutPixFB(x, y, 0);
            }
        }
        PSF1_FontPtr++;
    }
}

void BasicRenderer::HandleScrolling() {
    size_t fbSize = targetFramebuffer->common.framebuffer_pitch * targetFramebuffer->common.framebuffer_height;

    memmove(backbuffer,
        (void*)((uintptr_t)backbuffer + 64 * (globalRenderer->targetFramebuffer->common.framebuffer_pitch / (globalRenderer->targetFramebuffer->common.framebuffer_bpp / 8))),
        (targetFramebuffer->common.framebuffer_height) * targetFramebuffer->common.framebuffer_width * sizeof(uint32_t));

    // Clear the last row to avoid artifacts
    
    memcpy((void*)backbuffer, backbuffer, fbSize);

    for (int y = 0; y < 16; y++){
        for (int x = 0; x < targetFramebuffer->common.framebuffer_width; x++) {
            PutPix(x, (targetFramebuffer->common.framebuffer_height-1) - y, clr);
        }
    }

    
    cursorPos.Y -= 16;
}

void BasicRenderer::print(unsigned int color, const char* str){
    if (!status) return;

    char* chr = (char*)str;
    while (*chr != 0){
        if (cursorPos.Y+16 >= targetFramebuffer->common.framebuffer_height) {
            HandleScrolling();
        }

        // Draw character if within bounds
        if (*chr != '\n') {
            drawChar(color, *chr, cursorPos.X, cursorPos.Y);
        }

        cursorPos.X += 8; // Move cursor to the right
        if ((cursorPos.X + 8) > cursorPos.XLim || (*chr == '\n')) {
            cursorPos.X = cursorPos.overX; // Move back to start of the line
            cursorPos.Y += 16; // Move down to the next line

            if (*chr == '\n'){
                serialPrint(COM1, "\n\r");
            }
        }
        chr++;
    }
}
void BasicRenderer::printf(unsigned int color, const char* str, ...){
    va_list args;
    va_start(args, str);
    printfva(color, str, args);
    va_end(args);
}

void BasicRenderer::printf(const char* str, ...){
    va_list args;
    va_start(args, str);
    printfva(0xFFFFFF, str, args);
    va_end(args);
}

void BasicRenderer::printfva(uint32_t color, const char* str, va_list args){
    if (!status) return;


    char* chr = (char*)str;

    while (*chr != 0){
        if (*chr == '%'){
            chr++;
            switch (*chr){
                case 's':{ //%s string
                    const char* str = va_arg(args, char*);
                    print(color, str);
                    chr++;
                    break;
                }
                case 'd':{
                    const char* str = toString((uint32_t)va_arg(args, int));
                    print(color, str);
                    chr++;
                    break;
                }
                case 'f':{
                    const char* str = toString((double)va_arg(args, double));
                    print(color, str);
                    chr++;
                    break;
                }
                case 'u':{// %u unsigned int
                    const char* str = toString((uint32_t)va_arg(args, int));
                    print(color, str);
                    chr++;
                    break;
                }
                case 'x':{// %x unsigned hex | TODO: Add support for lowercase (32 bit)
                    const char* str = toHString((uint32_t)va_arg(args, uint32_t));
                    print(color, str);
                    chr++;
                    break;
                }
                case 'X':{// %X unsigned hex uppercase (32 bit)
                    const char* str = toHString((uint32_t)va_arg(args, uint32_t));
                    print(color, str);
                    chr++;
                    break;
                }
                case 'h':{// %h unsigned hex (16 bit)
                    chr++;
                    if (*chr == 'h'){ //%hh unsigned hex (8 bit)
                        const char* str = toHString((uint8_t)va_arg(args, int));
                        print(color, str);
                        chr++;
                        break;
                    }else{ //16 bit
                        const char* str = toHString((uint16_t)va_arg(args, int));
                        print(color, str);
                        break;
                    }
                }
                case 'l':{// %l long unsigned decimal (32 bit)
                    chr++;
                    if (*chr == 'l'){
                        chr++;
                        if (*chr == 'x' || *chr == 'X'){ // %llx Unsigned hex (64 bit)
                            const char* str = toHString((uint64_t)va_arg(args, uint64_t));
                            print(color, str);
                            chr++;
                            break;
                        }else{ // %ll long unsigned decimal (64 bit)
                            const char* str = toString((uint64_t)va_arg(args, uint64_t));
                            print(color, str);
                            break;
                        }
                    }else{ //16 bit
                        const char* str = toString((uint32_t)va_arg(args, uint32_t));
                        print(color, str);
                        break;
                    }
                }
                case 'c':{
                    chr++;
                    char arg = va_arg(args, int);
                    char st[2] = { arg, '\0'};
                    print(color, st);
                    break;
                }
            }
        continue;
        }

        print(color, charToConstCharPtr(*chr));
        chr++;
    }
    updateScreen();
}

void BasicRenderer::print(const char* str){
    print(0xFFFFFF, str);
}

void BasicRenderer::DrawPixels(unsigned int xStart, unsigned int xEnd, unsigned int yStart, unsigned int yEnd, unsigned int color) {
    if (!status) return;
    unsigned int BBP = 4; // Bytes per pixel (assuming 32-bit color)

    // Fill the pixel area
    for (unsigned int y = yStart; y < yEnd; y++) {
        for (unsigned int x = xStart; x < xEnd; x++) {
            // Calculate the pixel address
            unsigned int* pixelAddress = (unsigned int*)((unsigned int*)backbuffer + x + (y * (globalRenderer->targetFramebuffer->common.framebuffer_pitch / (globalRenderer->targetFramebuffer->common.framebuffer_bpp / 8))));
            *pixelAddress = color; // Set the pixel color
        }
    }
}

void BasicRenderer::Clear(unsigned int color){
    if (!status) return;
    DrawPixels(0, targetFramebuffer->common.framebuffer_width, 0, targetFramebuffer->common.framebuffer_height, color);
    clr = color;
}
void BasicRenderer::PutPix(uint32_t X, uint32_t Y, uint32_t Colour){
    if (X >= targetFramebuffer->common.framebuffer_width || Y >= targetFramebuffer->common.framebuffer_height) return;
    uint8_t* base = (uint8_t*)backbuffer;
    uint32_t* pixel = (uint32_t*)(base + (Y * targetFramebuffer->common.framebuffer_pitch) + (X * (targetFramebuffer->common.framebuffer_bpp / 8)));
    *pixel = Colour;
}

void BasicRenderer::PutPixFB(uint32_t X, uint32_t Y, uint32_t Colour){
    if (X >= targetFramebuffer->common.framebuffer_width || Y >= targetFramebuffer->common.framebuffer_height) return;
    uint8_t* base = (uint8_t*)targetFramebuffer->common.framebuffer_addr;
    uint32_t* pixel = (uint32_t*)(base + (Y * targetFramebuffer->common.framebuffer_pitch) + (X * (targetFramebuffer->common.framebuffer_bpp / 8)));
    *pixel = Colour;
}

uint32_t BasicRenderer::GetPix(uint32_t X, uint32_t Y){
    uint8_t* base = (uint8_t*)backbuffer;
    uint32_t* pixel = (uint32_t*)(base + (Y * targetFramebuffer->common.framebuffer_pitch) + (X * (targetFramebuffer->common.framebuffer_bpp / 8)));
    return *pixel;
}
bool MouseDrawn = false;
void BasicRenderer::ClearMouseCursor(uint8_t* MouseCursor, Point Position){
    //if (status == false) return;
    if (!MouseDrawn) return;

    int XMax = 16;
    int YMax = 16;
    int DifferenceX = targetFramebuffer->common.framebuffer_width - Position.X;
    int DifferenceY = targetFramebuffer->common.framebuffer_height - Position.Y;

    if (DifferenceX < 16) XMax = DifferenceX;
    if (DifferenceY < 16) YMax = DifferenceY;

    for (int Y = 0; Y < YMax; Y++){
        for (int X = 0; X < XMax; X++){
            int Bit = Y * 16 + X;
            int Byte = Bit / 8;
            if ((MouseCursor[Byte] & (0b10000000 >> (X % 8))))
            {
                if (GetPix(Position.X + X, Position.Y + Y) == MouseCursorBufferAfter[X + Y *16]){
                    PutPix(Position.X + X, Position.Y + Y, MouseCursorBuffer[X + Y * 16]);
                }
            }
        }
    }
}

void BasicRenderer::DrawOverlayMouseCursor(uint8_t* MouseCursor, Point Position, uint32_t Colour){
    /*if (status == false) {
        g_AdvRend->DrawOverlayMouseCursor(MouseCursor, Position, Colour);
        return;
    };*/
    /*int XMax = 16;
    int YMax = 16;
    int DifferenceX = targetFramebuffer->common.framebuffer_width - Position.X;
    int DifferenceY = targetFramebuffer->common.framebuffer_height - Position.Y;

    if (DifferenceX < 16) XMax = DifferenceX;
    if (DifferenceY < 16) YMax = DifferenceY;

    for (int Y = 0; Y < YMax; Y++){
        for (int X = 0; X < XMax; X++){
            int Bit = Y * 16 + X;
            int Byte = Bit / 8;
            if ((MouseCursor[Byte] & (0b10000000 >> (X % 8))))
            {
                MouseCursorBuffer[X + Y * 16] = GetPix(Position.X + X, Position.Y + Y);
                PutPix(Position.X + X, Position.Y + Y, Colour);
                MouseCursorBufferAfter[X + Y * 16] = GetPix(Position.X + X, Position.Y + Y);

            }
        }
    }

    MouseDrawn = true;*/
}
void BasicRenderer::ClearChar(unsigned int color){
    if (!status) return;

    if (cursorPos.X == 0){
        cursorPos.X = targetFramebuffer->common.framebuffer_width;
        cursorPos.Y -= 16;
        if (cursorPos.Y < 0) cursorPos.Y = 0;
    }

    unsigned int XOffset = cursorPos.X;
    unsigned int YOffset = cursorPos.Y;

    unsigned int* PixelPtr = (unsigned int*)backbuffer;
    for (unsigned long Y = YOffset; Y < YOffset + 16; Y++){
        for (unsigned long X = XOffset - 8; X < XOffset; X++){
                    *(unsigned int*)(PixelPtr + X + (Y * (globalRenderer->targetFramebuffer->common.framebuffer_pitch / (globalRenderer->targetFramebuffer->common.framebuffer_bpp / 8)))) = color;
        }
    }

    cursorPos.X -= 8;

    if (cursorPos.X < 0){
        cursorPos.X = targetFramebuffer->common.framebuffer_width;
        cursorPos.Y -= 16;
        if (cursorPos.Y < 0) cursorPos.Y = 0;
    }

}


void kprintf(unsigned int color, const char* str, ...){
    va_list args;
    va_start(args, str);
    globalRenderer->printfva(color, str, args);
    va_end(args);
}

void kprintf(const char* str, ...){
    va_list args;
    va_start(args, str);
    globalRenderer->printfva(0xFFFFFF, str, args);
    va_end(args);
}

void DrawBMPScaled(int dstX, int dstY, int targetW, int targetH, uint8_t* bmpData, void* buffer, uint64_t pitch) {
    BMPFileHeader* fileHeader = (BMPFileHeader*)bmpData;
    BMPInfoHeader* infoHeader = (BMPInfoHeader*)(bmpData + sizeof(BMPFileHeader));

    if (fileHeader->bfType != 0x4D42 || infoHeader->biBitCount != 24 || infoHeader->biCompression != 0) {
        kprintf("INVALID BMP");
        return;
    }

    int srcW = infoHeader->biWidth;
    int srcH = infoHeader->biHeight;
    uint8_t* pixelData = bmpData + fileHeader->bfOffBits;
    int rowSize = ((srcW * 3 + 3) / 4) * 4;

    for (int y = 0; y < targetH; y++) {
        int destY = dstY + y;

        for (int x = 0; x < targetW; x++) {
            int destX = dstX + x;

            int srcX = (x * srcW) / targetW;
            int srcY = (y * srcH) / targetH;

            uint8_t* pixel = pixelData + (srcH - 1 - srcY) * rowSize + srcX * 3;

            uint8_t blue  = pixel[0];
            uint8_t green = pixel[1];
            uint8_t red   = pixel[2];

            uint32_t color = 0xFF000000 | (red << 16) | (green << 8) | blue;
            *(uint32_t*)((uint8_t*)buffer + destY * pitch + destX * 4) = color;
        }
    }
}
