#pragma once
#include <stdint.h>

void io_wait();
void outb (uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
void outw(uint16_t port, uint16_t value);
uint16_t inw(uint16_t port);
void outd(uint16_t port, uint32_t value);
uint32_t ind(uint16_t port);



void mmio_outb(uint32_t base, uint8_t value);

void mmio_outw(uint32_t base, uint16_t value);

void mmio_outd(uint32_t base, uint32_t value);

uint8_t mmio_inb(uint32_t base);

uint16_t mmio_inw(uint32_t base);

uint32_t mmio_ind(uint32_t base);