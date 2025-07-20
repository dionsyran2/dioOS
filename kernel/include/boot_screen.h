#pragma once

#ifdef BOOT_SCREEN

#include "boot_logo.h"
#include "BasicRenderer.h"

bool boolean_flag_1 = false;

void bootTest(){
    if (globalRenderer->status) return;
    
    uint8_t* buffer = (uint8_t*)globalRenderer->targetFramebuffer->common.framebuffer_addr;
    uint32_t width = globalRenderer->targetFramebuffer->common.framebuffer_width;
    uint32_t height = globalRenderer->targetFramebuffer->common.framebuffer_height;
    uint32_t pitch = globalRenderer->targetFramebuffer->common.framebuffer_pitch;

    uint32_t bar_width = 256;
    uint32_t bar_heigth = 26;

    int X = (width - bar_width) / 2;
    int Y = ((height - 132 - bar_heigth) / 2) + 132 + 50;

    uint32_t box_width = 12;
    uint32_t box_height = 22;
    uint32_t box_y = 2;

    DrawBMPScaled((width - 290) / 2, (height - 132 - bar_heigth) / 2, 290, 132, boot_logo_bmp, buffer, pitch);

    int c_pos = -2; //in blocks
    for (int y = Y; y < Y + bar_heigth; y++) {
        for (int x = X; x < X + bar_width; x++) {
            if ((y - Y) < 2 || (x - X) < 2 || (y - Y) >= (bar_heigth - 2) || (x - X) >= (bar_width - 2)) {
                uint32_t* pixel = (uint32_t*)((uint8_t*)buffer + (y * pitch) + (x * 4));
                *pixel = 0xFFFFFF;
            }
        }
    }


    while (1) {
        for (int y = 2; y < bar_heigth - 2; y++) {
            for (int x = 2; x < bar_width - 2; x++) {
                uint32_t* pixel = (uint32_t*)(buffer + ((y + Y) * pitch) + ((x + X) * 4));
                *pixel = 0x000000;
            }
        }

        for (int i = 0; i < 3; i++) {
            int blockX = (c_pos + i) * (box_width + 2); // +2 for spacing
            if (blockX < 0 || blockX + box_width > (int)bar_width - 2) continue;

            for (uint32_t y = 0; y < box_height; y++) {
                for (uint32_t x = 0; x < box_width; x++) {
                    int px = X + 2 + blockX + x;
                    int py = Y + box_y + y;
                    if (px >= 0 && px < (int)width && py >= 0 && py < (int)height) {
                        uint32_t* pixel = (uint32_t*)(buffer + (py * pitch) + (px * 4));
                        *pixel = 0xB23505;
                    }
                }
            }
        }

        c_pos++;
        if (c_pos > (int)(bar_width / (box_width + 2))) c_pos = -2;

        Sleep(100);

        if (boolean_flag_1) {
            boolean_flag_1 = false;
            break;
        }
    }
}

#endif