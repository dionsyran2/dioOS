#include "serial.h"

void InitSerial(uint16_t port){
    outb(port + 1, 0x00);
    
    outb(port + 3, 0x80);
    outb(port + 0, 1);
    outb(port + 1, 0);

    outb(port + 3, 0x03);

    outb(port + 2, 0xC7);

    outb(port + 1, 0x01);
}

void serialWrite(uint16_t port, char c) {
    while ((inb(port + 5) & 0x20) == 0);
    outb(port, c);
}
void serialPrint(uint16_t port, const char* str) {
    while (*str) {
        serialWrite(port, *str++);
    }
}

//read etc...