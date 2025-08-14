#include <cstr.h>
#include <math.h>
#include <drivers/framebuffer/framebuffer.h>
#include <memory/heap.h>

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

#include <rendering/ttf.h>


unsigned char* TTF::getBitmap(wchar_t chr, uint8_t size, int* width, int* height, int *x0, int *y0) {
    float scale = stbtt_ScaleForPixelHeight(&info, size);

    int x1, y1;

    float shift_x , shift_y;
    shift_x = shift_y = stbtt__oversample_shift(2);

    stbtt_GetCodepointBitmapBox(&info, chr, scale, scale, x0, y0, &x1, &y1);

    *width = x1 - *x0;
    *height = y1 - *y0;


    unsigned char* bitmap = (unsigned char*)malloc((*width) * (*height));

    stbtt_MakeCodepointBitmap(&info, bitmap, *width, *height, *width, scale, scale, chr);
    return bitmap;
}

void TTF::draw_char(wchar_t chr, uint32_t X, uint32_t Y, uint8_t size, uint32_t color, display* screen) {
    float scale = stbtt_ScaleForPixelHeight(&info, size);
    int width, height, x0, y0;
    int baseline = (int)(ascent * scale);

    unsigned char* bitmap = getBitmap(chr, size, &width, &height, &x0, &y0);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            unsigned char pixel = bitmap[y * width + x];
            
            if (pixel > 0) {
                
                int drawX = X + x0 + x;
                int drawY = Y + (baseline + y0) + y;

                uint32_t currentColor = screen->get_pixel(drawX, drawY);

                uint8_t existingAlpha = (currentColor >> 24) & 0xFF;
                uint8_t textAlpha = pixel;
                uint8_t textAlphaChannel = (color >> 24) & 0xFF;
                uint32_t finalAlpha = (textAlphaChannel * textAlpha + existingAlpha * (255 - textAlpha) / 255);
                
                uint8_t blendedR = (color & 0xFF) * textAlpha / 255 + (currentColor & 0xFF) * (255 - textAlpha) / 255;
                uint8_t blendedG = ((color >> 8) & 0xFF) * textAlpha / 255 + ((currentColor >> 8) & 0xFF) * (255 - textAlpha) / 255;
                uint8_t blendedB = ((color >> 16) & 0xFF) * textAlpha / 255 + ((currentColor >> 16) & 0xFF) * (255 - textAlpha) / 255;

                uint32_t finalColor = (finalAlpha << 24) | (blendedB << 16) | (blendedG << 8) | blendedR;

                screen->set_pixel(drawX, drawY, finalColor);
            }
        }
    }


    free(bitmap);
}


uint32_t TTF::get_text_width(wchar_t* str, uint8_t size){
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


        if (*chr != '\n') {
            stbtt_GetCodepointHMetrics(&info, *chr, &advance_width, &left_side_bearing);

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