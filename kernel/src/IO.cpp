#include <stdint.h>
#include <IO.h>

void outb(uint16_t port, uint8_t value){
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port){
    uint8_t returnVal;
    asm volatile ("inb %1, %0"
    : "=a"(returnVal)
    : "Nd"(port));
    return returnVal;
}

void io_wait(){
    asm volatile ("outb %%al, $0x80" : : "a"(0));
}

void outw(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outd(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

uint32_t ind(uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}






void mmio_outb(uint32_t base, uint8_t value) {
 uint8_t *mem = (uint8_t *) (base);
 *mem = value;
}

void mmio_outw(uint32_t base, uint16_t value) {
 uint16_t *mem = (uint16_t *) (base);
 *mem = value;
}

void mmio_outd(uint32_t base, uint32_t value) {
 uint32_t *mem = (uint32_t *) (base);
 *mem = value;
}

uint8_t mmio_inb(uint32_t base) {
 uint8_t *mem = (uint8_t *) (base);
 return *mem;
}

uint16_t mmio_inw(uint32_t base) {
 uint16_t *mem = (uint16_t *) (base);
 return *mem;
}

uint32_t mmio_ind(uint32_t base) {
 uint32_t *mem = (uint32_t *) (base);
 return *mem;
}