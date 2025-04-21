#include "BasicRenderer.h"

BasicRenderer* globalRenderer;
BasicRenderer::BasicRenderer(multiboot_tag_framebuffer* targetFrameBuffer, PSF1_FONT* psf1_font)
{
    targetFramebuffer = targetFrameBuffer;
    PSF1_Font = psf1_font;
    cursorPos.X = 0;
    cursorPos.Y = 0;
    cursorPos.XLim = targetFrameBuffer->common.framebuffer_width-1;
    cursorPos.overX = 0;
};
/*
    GET RANDOM COLOR ----DELETE THAT LATER!!!
*/
// LCG parameters (these are commonly used values)
#define MODULUS 2147483648 // 2^31
#define MULTIPLIER 1103515245
#define INCREMENT 12345

static unsigned int seed = 1442; // Global seed variable
static uint64_t clr;
// Function to initialize the RNG
void BasicRenderer::Set(bool stat){
    status = stat;
}
void BasicRenderer::srand_custom(unsigned int initialSeed) {
    seed = initialSeed;
};

// Function to generate a random number
unsigned int BasicRenderer::rand_custom() {
    seed = (MULTIPLIER * seed + INCREMENT) % MODULUS;
    return seed;
}

// Function to generate a random hex color with alpha
unsigned int BasicRenderer::randHexColor() {
    // Generate random values for Alpha, Red, Green, Blue using custom rand
    unsigned int alpha = rand_custom() % 256;  // Random value for Alpha (0-255)
    unsigned int red = rand_custom() % 256;    // Random value for Red (0-255)
    unsigned int green = rand_custom() % 256;  // Random value for Green (0-255)
    unsigned int blue = rand_custom() % 256;   // Random value for Blue (0-255)

    // Combine ARGB values into a single hex color (0xAARRGGBB)
    unsigned int hexColor = (alpha << 24) | (red << 16) | (green << 8) | blue;

    return hexColor;
}





void BasicRenderer::drawChar(unsigned int color, char chr, unsigned int xOff, unsigned int yOff){
    if (!status) return;
    unsigned int* pixPtr = (unsigned int*)targetFramebuffer->common.framebuffer_addr;
    char* PSF1_FontPtr = (char*)PSF1_Font->glyphBuffer + (chr * PSF1_Font->psf1_Header->charsize);
    for (unsigned long y=yOff; y<yOff + 16; y++){
        for(unsigned long x = xOff; x<xOff + 8; x++){
            if((*PSF1_FontPtr & (0b10000000 >> (x - xOff))) > 0){
                *(unsigned int*)(pixPtr + x + (y * (globalRenderer->targetFramebuffer->common.framebuffer_pitch / (globalRenderer->targetFramebuffer->common.framebuffer_bpp / 8)))) = color;
            }
        }
        PSF1_FontPtr++;
    }
}

void BasicRenderer::HandleScrolling() {
    size_t fbSize = targetFramebuffer->common.framebuffer_pitch * targetFramebuffer->common.framebuffer_height;
    void* backBuffer = malloc(fbSize);
    memcpy(backBuffer, (void*)targetFramebuffer->common.framebuffer_addr, fbSize);
    memmove(backBuffer,
        backBuffer + 64 * (globalRenderer->targetFramebuffer->common.framebuffer_pitch / (globalRenderer->targetFramebuffer->common.framebuffer_bpp / 8)),
        (targetFramebuffer->common.framebuffer_height) * targetFramebuffer->common.framebuffer_width * sizeof(uint32_t));
    memcpy((void*)targetFramebuffer->common.framebuffer_addr, backBuffer, fbSize);
    /*// Shift all rows up by 16 pixels
    for (int y = 15; y < targetFramebuffer->common.framebuffer_height; y++) {
        for (int x = 0; x < targetFramebuffer->common.framebuffer_width; x++) {
            // Move the pixels up by 16 rows
            uint32_t pixel = GetPix(x, y); // Get the pixel from the framebuffer
            PutPix(x, y - 16, pixel); // Place it in the new location
        }
    }*/

    // Clear the last row to avoid artifacts
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
        }
        chr++;
    }
}
void BasicRenderer::printf(unsigned int color, const char* str, ...){
    if (!status) return;
    va_list args;
    va_start(args, str);
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
                case 'd':{// %d signed decimal integer | TODO: Add support for negative numbers
                    const char* str = toString((uint32_t)va_arg(args, int));
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
            }
        continue;
        }

        print(color, charToConstCharPtr(*chr));
        chr++;
    }
    va_end(args);
}
void BasicRenderer::printf(const char* str, ...){
    if (!status) return;
    va_list args;
    va_start(args, str);
    char* chr = (char*)str;

    while (*chr != 0){
        if (*chr == '%'){
            chr++;
            switch (*chr){
                case 's':{ //%s string
                    const char* str = va_arg(args, char*);
                    print(str);
                    chr++;
                    break;
                }
                case 'd':{// %d signed decimal integer | TODO: Add support for negative numbers
                    const char* str = toString((uint32_t)va_arg(args, int));
                    print(str);
                    chr++;
                    break;
                }
                case 'u':{// %u unsigned int
                    const char* str = toString((uint32_t)va_arg(args, int));
                    print(str);
                    chr++;
                    break;
                }
                case 'x':{// %x unsigned hex | TODO: Add support for lowercase (32 bit)
                    const char* str = toHString((uint32_t)va_arg(args, uint32_t));
                    print(str);
                    chr++;
                    break;
                }
                case 'X':{// %X unsigned hex uppercase (32 bit)
                    const char* str = toHString((uint32_t)va_arg(args, uint32_t));
                    print(str);
                    chr++;
                    break;
                }
                case 'h':{// %h unsigned hex (16 bit)
                    chr++;
                    if (*chr == 'h'){ //%hh unsigned hex (8 bit)
                        const char* str = toHString((uint8_t)va_arg(args, int));
                        print(str);
                        chr++;
                        break;
                    }else{ //16 bit
                        const char* str = toHString((uint16_t)va_arg(args, int));
                        print(str);
                        break;
                    }
                }
                case 'l':{// %l long unsigned decimal (32 bit)
                    chr++;
                    if (*chr == 'l'){
                        chr++;
                        if (*chr == 'x' || *chr == 'X'){ // %llx Unsigned hex (64 bit)
                            const char* str = toHString((uint64_t)va_arg(args, uint64_t));
                            print(str);
                            chr++;
                            break;
                        }else{ // %ll long unsigned decimal (64 bit)
                            const char* str = toString((uint64_t)va_arg(args, uint64_t));
                            print(str);
                            break;
                        }
                    }else{ //32 bit
                        const char* str = toString((uint32_t)va_arg(args, uint32_t));
                        print(str);
                        break;
                    }
                }
            }
        continue;
        }

        print(charToConstCharPtr(*chr));
        chr++;
    }
    va_end(args);
}

void BasicRenderer::print(const char* str){
    if (!status) return;
    char* chr = (char*)str;
    unsigned int color = 0xFFFFFFFF;
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
            if (cursorPos.Y+16 >= targetFramebuffer->common.framebuffer_height) {
                HandleScrolling();
            }else{
                cursorPos.Y += 16;
            }
        }
        chr++;
    }
}

uint64_t read_tsc() {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo; // Combine high and low parts
}

void BasicRenderer::delay(unsigned long milliseconds) {
    // Adjust this constant based on your CPU speed for better precision
    const uint64_t cpu_cycles_per_millisecond = 3800000; // Change this according to your CPU speed
    uint64_t start = read_tsc();
    uint64_t end = start + (milliseconds * cpu_cycles_per_millisecond);
    
    // Busy wait until the desired time has passed
    while (read_tsc() < end) {
        __asm__ volatile ("nop"); // Optional: Insert NOPs to reduce power consumption
    }
}
void BasicRenderer::DrawPixels(unsigned int xStart, unsigned int xEnd, unsigned int yStart, unsigned int yEnd, unsigned int color) {
    if (!status) return;
    unsigned int BBP = 4; // Bytes per pixel (assuming 32-bit color)

    // Fill the pixel area
    for (unsigned int y = yStart; y < yEnd; y++) {
        for (unsigned int x = xStart; x < xEnd; x++) {
            // Calculate the pixel address
            unsigned int* pixelAddress = (unsigned int*)((unsigned int*)targetFramebuffer->common.framebuffer_addr + x + (y * (globalRenderer->targetFramebuffer->common.framebuffer_pitch / (globalRenderer->targetFramebuffer->common.framebuffer_bpp / 8))));
            *pixelAddress = color; // Set the pixel color
        }
        delay(.1);
    }
}
void BasicRenderer::Clear(unsigned int color){
    if (!status) return;
    DrawPixels(0, targetFramebuffer->common.framebuffer_width, 0, targetFramebuffer->common.framebuffer_height, color);
    clr = color;
}
void BasicRenderer::PutPix(uint32_t X, uint32_t Y, uint32_t Colour){
    *(uint32_t*)((uint64_t)targetFramebuffer->common.framebuffer_addr + (X*4) + (Y * (globalRenderer->targetFramebuffer->common.framebuffer_pitch / (globalRenderer->targetFramebuffer->common.framebuffer_bpp / 8)) * 4)) = Colour;
}

uint32_t BasicRenderer::GetPix(uint32_t X, uint32_t Y){
    return *(uint32_t*)((uint64_t)targetFramebuffer->common.framebuffer_addr + (X*4) + (Y * (globalRenderer->targetFramebuffer->common.framebuffer_pitch / (globalRenderer->targetFramebuffer->common.framebuffer_bpp / 8)) * 4));
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

    unsigned int* PixelPtr = (unsigned int*)targetFramebuffer->common.framebuffer_addr;
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