/* Interrupt Descriptor Table */
#pragma once
#include <stdint.h>
#include <stddef.h>

struct interrupt_descriptor {
   uint16_t offset_1;        // offset bits 0..15
   uint16_t selector;        // a code segment selector in GDT or LDT
   uint8_t  ist;             // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
   uint8_t  type_attributes; // gate type, dpl, and p fields
   uint16_t offset_2;        // offset bits 16..31
   uint32_t offset_3;        // offset bits 32..63
   uint32_t zero;            // reserved

   // @brief Set offset, sets the address of the ISR (Interrupt Service Routine)
   void set_offset(uint64_t address);
};

struct interrupt_descriptor_table_t{
    // These 2 values are read directly by the cpu (They are part of the descriptor)
    uint16_t limit;
    uint64_t address;

    // This is where address points to
    // Its part of the same struct for simplicity
    interrupt_descriptor interrupt_table[256] __attribute__((aligned(0x10)));;

    interrupt_descriptor_table_t();
    void set_interrupt_handler(void* ISR, uint8_t vector, uint8_t type_attr, uint8_t selector);
    void load();
} __attribute__((packed));