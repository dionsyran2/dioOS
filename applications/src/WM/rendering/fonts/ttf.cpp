#include "cstr.h"
#include "libc.h"
#include "math.h"

void stub(int x)
{
    return;
}

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_ifloor(x) ((int)floor(x))
#define STBTT_iceil(x) ((int)ceil(x))

#define STBTT_sqrt(x) sqrt(x)
#define STBTT_pow(x, y) pow(x, y)

#define STBTT_fmod(x, y) fmod(x, y)

#define STBTT_cos(x) cos(x)
#define STBTT_acos(x) acos(x)

#define STBTT_fabs(x) fabs(x)

#define STBTT_malloc(x, u) malloc(x)
#define STBTT_free(x, u) free(x)    

#define STBTT_assert(x) stub(x);

#define STBTT_strlen(x) strlen(x)

#define STBTT_memcpy memcpy
#define STBTT_memset memset

#include "ttf.h"

unsigned char* TTF::getBitmap(wchar_t chr, uint8_t size, int* width, int* height, int *x0, int *y0) {
    float scale = stbtt_ScaleForPixelHeight(&info, size);

    int x1, y1;

    float shift_x = 0.0f, shift_y = 0.0f;

    stbtt_GetCodepointBitmapBoxSubpixel(&info, chr, scale, scale, shift_x, shift_y, x0, y0, &x1, &y1);

    *width = x1 - *x0;
    *height = y1 - *y0;


    unsigned char* bitmap = (unsigned char*)malloc((*width) * (*height));

    stbtt_MakeCodepointBitmapSubpixel(&info, bitmap, *width, *height, *width, scale, scale, shift_x, shift_y, chr);

    return bitmap;
}

void TTF::drawChar(wchar_t chr, uint32_t X, uint32_t Y, uint32_t pitch, void* buffer, uint8_t size, uint32_t color) {
    float scale = stbtt_ScaleForPixelHeight(&info, size);
    int width, height, x0, y0;

    // Get the bitmap for the character with oversampling
    unsigned char* bitmap = getBitmap(chr, size, &width, &height, &x0, &y0);

    // Loop through the oversampled bitmap and draw it directly at the correct position
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            unsigned char pixel = bitmap[y * width + x];  // Get the grayscale pixel (0-255)
            
            if (pixel > 0) {  // If the pixel is not transparent
                // Mapping coordinates to final screen resolution
                int drawX = X + x0 + x;  // Scale down to original resolution
                int drawY = Y + y0 + y;  // Scale down to original resolution

                // Calculate the new color with transparency (alpha blending)
                uint32_t currentColor = *(uint32_t*)((uint32_t*)buffer + drawX + (drawY * pitch));

                uint8_t existingAlpha = (currentColor >> 24) & 0xFF;
                uint8_t textAlpha = pixel;  // Assuming pixel is 0-255 and is the alpha value of the text
                
                // Blend the colors based on alpha
                uint8_t blendedR = (color & 0xFF) * textAlpha / 255 + (currentColor & 0xFF) * (255 - textAlpha) / 255;
                uint8_t blendedG = ((color >> 8) & 0xFF) * textAlpha / 255 + ((currentColor >> 8) & 0xFF) * (255 - textAlpha) / 255;
                uint8_t blendedB = ((color >> 16) & 0xFF) * textAlpha / 255 + ((currentColor >> 16) & 0xFF) * (255 - textAlpha) / 255;

                uint32_t finalColor = (existingAlpha << 24) | (blendedB << 16) | (blendedG << 8) | blendedR;

                *(uint32_t*)((uint32_t*)buffer + drawX + (drawY * pitch)) = finalColor;
            }
        }
    }


    free(bitmap);
}




void TTF::print(unsigned int color, const wchar_t* str, uint32_t x, uint32_t y, uint32_t xlim, uint32_t pitch, void* buffer, uint8_t size) {
    float scale = stbtt_ScaleForPixelHeight(&info, size);

    int baseline = (int)(ascent * scale);
    int offset = (int)(descent * scale);

    wchar_t* chr = (wchar_t*)str;
    int yPos = y + baseline;
    int xPos = x;

    int prev_char = -1;

    while (*chr != 0) {
        if (prev_char != -1) {
            int kern = stbtt_GetCodepointKernAdvance(&info, prev_char, *chr);
            if (kern > -100 && kern < 100) {
                xPos += kern * scale;
            }
        }


        if (*chr != '\n' && *chr != ' ') {
            drawChar(*chr, xPos, yPos, pitch, buffer, size, color);
            stbtt_GetCodepointHMetrics(&info, *chr, &advance_width, &left_side_bearing);

            xPos += advance_width * scale;
        }


        if (*chr == ' ') {
            stbtt_GetCodepointHMetrics(&info, ' ', &advance_width, &left_side_bearing);
            xPos += advance_width * scale;
        }


        if ((xPos + advance_width * scale) > xlim || (*chr == '\n')) {
            xPos = x;
            yPos += size;
        }

        prev_char = *chr;
        chr++;
    }
}

uint32_t TTF::getTextLength(wchar_t* str, uint8_t size){
    float scale = stbtt_ScaleForPixelHeight(&info, size);

    int baseline = (int)(ascent * scale);
    int offset = (int)(descent * scale);

    wchar_t* chr = (wchar_t*)str;
    uint32_t xSize = 0;

    int prev_char = -1;

    while (*chr != 0) {
        if (prev_char != -1) {
            int kern = stbtt_GetCodepointKernAdvance(&info, prev_char, *chr);
            if (kern > -100 && kern < 100) {
                xSize += kern * scale;
            }
        }


        if (*chr != '\n' && *chr != ' ') {
            stbtt_GetCodepointHMetrics(&info, *chr, &advance_width, &left_side_bearing);

            xSize += advance_width * scale;
        }


        if (*chr == ' ') {
            stbtt_GetCodepointHMetrics(&info, ' ', &advance_width, &left_side_bearing);
            xSize += advance_width * scale;
        }

        prev_char = *chr;
        chr++;
    }
    return xSize;
}


TTF::TTF(void* font){
    memset(&info, 0, sizeof(stbtt_fontinfo));
    this->font = font;
    stbtt_InitFont(&info, (const unsigned char *)font, 0);
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
}

TTF::~TTF(){
    
}
