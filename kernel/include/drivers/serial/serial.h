/* Definitions for the serial driver */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <line_discipline/line_discipline.h>

#define SERIAL_SCRATCH_REGISTER(base) (base + 7)

void InitSerial();

class serial_port : public tty_device_t {
    public:
    uint16_t address;

    void write(const char *data, size_t size, bool onlcr);
    int ioctl(int op, char *argp);
    void apply_termios(termios *state);
    void handle_interrupt();

    serial_port(uint16_t address, uint8_t irq);

};

void serialf(const char* str, ...);
void serialfva(const char* str, va_list args);