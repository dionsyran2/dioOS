#pragma once
#include <stdint.h>
/********************* NORMAL MOUSE POINTER *************************/

#ifdef MouseIcons
// 'MouseOutline', 10x16px
uint8_t MouseOutline[] = {
    0xe0, 0x00, 0xf0, 0x00,
    0xf8, 0x00, 0xdc, 0x00,
    0xdc, 0x00, 0xc6, 0x00,
    0xc7, 0x00, 0xc3, 0x80, 
    0xc1, 0xc0, 0xc0, 0xc0,
    0xc3, 0xc0, 0xc3, 0x80,
    0xfb, 0x00, 0xff, 0x00,
    0xcf, 0x80, 0x0f, 0x80
};

// 'MouseFill', 10x16px
uint8_t MouseFill[] = {
    0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x60, 0x00,
    0x70, 0x00, 0x78, 0x00,
    0x7c, 0x00, 0x7e, 0x00, 
    0x7f, 0x00, 0x7f, 0x80,
    0x7e, 0x00, 0x7c, 0x00,
    0x4c, 0x00, 0x06, 0x00,
    0x02, 0x00, 0x00, 0x00
};
#endif