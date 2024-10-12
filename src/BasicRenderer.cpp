#include "BasicRenderer.h"

BasicRenderer* globalRenderer;
BasicRenderer::BasicRenderer(Framebuffer* targetFrameBuffer, PSF1_FONT* psf1_font)
{
    targetFramebuffer = targetFrameBuffer;
    PSF1_Font = psf1_font;
    cursorPos.X = 0;
    cursorPos.Y = 0;
    cursorPos.XLim = targetFrameBuffer->Width-1;
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
/*END OF MY STUPIDITY*/



void BasicRenderer::drawChar(unsigned int color, char chr, unsigned int xOff, unsigned int yOff){
    unsigned int* pixPtr = (unsigned int*)targetFramebuffer->BaseAddress;
    char* PSF1_FontPtr = (char*)PSF1_Font->glyphBuffer + (chr * PSF1_Font->psf1_Header->charsize);
    for (unsigned long y=yOff; y<yOff + 16; y++){
        for(unsigned long x = xOff; x<xOff + 8; x++){
            if((*PSF1_FontPtr & (0b10000000 >> (x - xOff))) > 0){
                *(unsigned int*)(pixPtr + x + (y * targetFramebuffer->PixelsPerScanLine)) = color;
            }
        }
        PSF1_FontPtr++;
    }
}

void BasicRenderer::HandleScrolling() {

    // Shift all rows up by 16 pixels
    for (int y = 15; y < targetFramebuffer->Height; y++) {
        for (int x = 0; x < targetFramebuffer->Width; x++) {
            // Move the pixels up by 16 rows
            uint32_t pixel = GetPix(x, y); // Get the pixel from the framebuffer
            PutPix(x, y - 16, pixel); // Place it in the new location
        }
    }

    // Clear the last row to avoid artifacts
    for (int y = 0; y < 20; y++){
        for (int x = 0; x < targetFramebuffer->Width; x++) {
            PutPix(x, (targetFramebuffer->Height-1) - y, clr); // Assuming 0 is the clear color
        }
    }
    cursorPos.Y -= 16;

}

void BasicRenderer::print(unsigned int color, const char* str){
    char* chr = (char*)str;
    while (*chr != 0){
        if (cursorPos.Y+16 >= targetFramebuffer->Height) {
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


void BasicRenderer::print(const char* str){
    char* chr = (char*)str;
    unsigned int color = 0xFFFFFFFF;
    while (*chr != 0){
        if (cursorPos.Y+16 >= targetFramebuffer->Height) {
            HandleScrolling();
        }

        // Draw character if within bounds
        if (*chr != '\n') {
            drawChar(color, *chr, cursorPos.X, cursorPos.Y);
        }

        cursorPos.X += 8; // Move cursor to the right
        if ((cursorPos.X + 8) > cursorPos.XLim || (*chr == '\n')) {
            cursorPos.X = cursorPos.overX; // Move back to start of the line
            if (cursorPos.Y+16 >= targetFramebuffer->Height) {
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
    unsigned int BBP = 4; // Bytes per pixel (assuming 32-bit color)

    // Fill the pixel area
    for (unsigned int y = yStart; y < yEnd; y++) {
        for (unsigned int x = xStart; x < xEnd; x++) {
            // Calculate the pixel address
            unsigned int* pixelAddress = (unsigned int*)((unsigned int*)targetFramebuffer->BaseAddress + x + (y * targetFramebuffer->PixelsPerScanLine));
            *pixelAddress = color; // Set the pixel color
        }
        delay(.1);
    }
}
void BasicRenderer::Clear(unsigned int color){
    DrawPixels(0, targetFramebuffer->Width, 0, targetFramebuffer->Height, color);
    clr = color;
}
void BasicRenderer::PutPix(uint32_t X, uint32_t Y, uint32_t Colour){
    *(uint32_t*)((uint64_t)targetFramebuffer->BaseAddress + (X*4) + (Y * targetFramebuffer->PixelsPerScanLine * 4)) = Colour;
}

uint32_t BasicRenderer::GetPix(uint32_t X, uint32_t Y){
    return *(uint32_t*)((uint64_t)targetFramebuffer->BaseAddress + (X*4) + (Y * targetFramebuffer->PixelsPerScanLine * 4));
}
bool MouseDrawn = false;
void BasicRenderer::ClearMouseCursor(uint8_t* MouseCursor, Point Position){
    if (!MouseDrawn) return;

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
                if (GetPix(Position.X + X, Position.Y + Y) == MouseCursorBufferAfter[X + Y *16]){
                    PutPix(Position.X + X, Position.Y + Y, MouseCursorBuffer[X + Y * 16]);
                }
            }
        }
    }
}

void BasicRenderer::DrawOverlayMouseCursor(uint8_t* MouseCursor, Point Position, uint32_t Colour){
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
                MouseCursorBuffer[X + Y * 16] = GetPix(Position.X + X, Position.Y + Y);
                PutPix(Position.X + X, Position.Y + Y, Colour);
                MouseCursorBufferAfter[X + Y * 16] = GetPix(Position.X + X, Position.Y + Y);

            }
        }
    }

    MouseDrawn = true;
}
void BasicRenderer::ClearChar(unsigned int color){

    if (cursorPos.X == 0){
        cursorPos.X = targetFramebuffer->Width;
        cursorPos.Y -= 16;
        if (cursorPos.Y < 0) cursorPos.Y = 0;
    }

    unsigned int XOffset = cursorPos.X;
    unsigned int YOffset = cursorPos.Y;

    unsigned int* PixelPtr = (unsigned int*)targetFramebuffer->BaseAddress;
    for (unsigned long Y = YOffset; Y < YOffset + 16; Y++){
        for (unsigned long X = XOffset - 8; X < XOffset; X++){
                    *(unsigned int*)(PixelPtr + X + (Y * targetFramebuffer->PixelsPerScanLine)) = color;
        }
    }

    cursorPos.X -= 8;

    if (cursorPos.X < 0){
        cursorPos.X = targetFramebuffer->Width;
        cursorPos.Y -= 16;
        if (cursorPos.Y < 0) cursorPos.Y = 0;
    }

}
