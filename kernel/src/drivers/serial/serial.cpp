#include <drivers/serial/serial.h>
#include <IO.h>
#include <cstr.h>

// @brief Initializes the Serial Port
void InitSerial(uint16_t port){
    outb(port + 1, 0x00);
    
    outb(port + 3, 0x80);
    outb(port + 0, 1);
    outb(port + 1, 0);

    outb(port + 3, 0x03);

    outb(port + 2, 0xC7);

    outb(port + 1, 0x01);
}

// @brief Writes one byte to the serial port
void serialWrite(uint16_t port, char c) {
    while ((inb(port + 5) & 0x20) == 0);
    outb(port, c);
}

// @brief Prints a string on the serial port
void serialPrint(uint16_t port, const char* str) {
    while (*str) {
        serialWrite(port, *str++);
    }
}

#include <printf.h>

void serialfva(const char* str, va_list args){
    if (str == nullptr) return;
    
    const uint64_t buffer_size = 2048;
    char buffer[buffer_size] = { 0 };
    int written = vsnprintf_(buffer, buffer_size, str, args);
    serialPrint(COM1, buffer);
}

#include <scheduling/spinlock/spinlock.h>
spinlock_t seriallock;
// @brief Prints a formatted string to the serial port
void serialf(const char* str, ...){
    uint64_t rflags = spin_lock(&seriallock);
    
    va_list args;
    va_start(args, str);
    serialfva(str, args);
    va_end(args);

    spin_unlock(&seriallock, rflags);
}