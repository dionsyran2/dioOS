/* This header contains helper functions for I/O Operations */
#pragma once

#include <stdint.h>
#include <stddef.h>

/* LEGACY I/O */
inline void outb(uint16_t port, uint8_t value){
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline uint8_t inb(uint16_t port){
    uint8_t returnVal;
    asm volatile ("inb %1, %0"
    : "=a"(returnVal)
    : "Nd"(port));
    return returnVal;
}

inline void io_wait(){
    asm volatile ("outb %%al, $0x80" : : "a"(0));
}

inline void outw(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

inline void outd(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

inline uint32_t ind(uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* PCI I/O */
/* TODO! */