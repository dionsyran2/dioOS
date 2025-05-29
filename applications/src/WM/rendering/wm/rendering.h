#pragma once
#include <stdint.h>
#include <stddef.h>
#include "window.h"

#define WIN_TITLE_BAR_HEIGHT 25
#define WIN_BORDER_RADIUS 6
#define WIN_BORDER_WIDTH 1

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


void PutPixInBB(uint32_t x, uint32_t y, uint32_t color);

uint32_t GetPixInBB(uint32_t x, uint32_t y);

void PutPixInFB(uint32_t x, uint32_t y, uint32_t color);

uint32_t GetPixInFB(uint32_t x, uint32_t y);

void RedrawScreen(int x, int y, uint32_t width, uint32_t height);

window_internal_t* getTopWindow(uint32_t x, uint32_t y);

bool isMouseOnTitlebar(int x, int y, window_internal_t* win);

int HandleMove(window_internal_t* window, int mX, int mY, bool released);

void DrawBMPScaled(int dstX, int dstY, int targetW, int targetH, uint8_t* bmpData, void* buffer, uint64_t pitch);

bool insideCornerFull(int x, int y, int winWidth, int winHeight, int radius);

bool isMouseOnXBtn(int x, int y, window_internal_t* win);

void UpdateScreen();

bool isWindowPixOnTop(window_internal_t *window, uint32_t x, uint32_t y);

extern bool _int_moving_window;
extern bool _int_render_bb_lock;
extern bool _int_finished_copying_bb_to_fb;