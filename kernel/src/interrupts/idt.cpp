#include <interrupts/idt.h>
#include <memory.h>

void interrupt_descriptor::set_offset(uint64_t ISR){
    this->offset_1 = ISR & 0xFFFF;
    this->offset_2 = (ISR >> 16) & 0xFFFF;
    this->offset_3 = (ISR >> 32) & 0xFFFFFFFF;
}

interrupt_descriptor_table_t::interrupt_descriptor_table_t(){
    this->address = (uint64_t)interrupt_table;
    this->limit = 0x0FFF;

    memset(interrupt_table, 0, sizeof(interrupt_table));
}

void interrupt_descriptor_table_t::set_interrupt_handler(void* ISR, uint8_t vector, uint8_t type_attr, uint8_t selector){
    interrupt_descriptor* descriptor = &interrupt_table[vector];
    descriptor->set_offset((uint64_t)ISR);
    descriptor->type_attributes = type_attr;
    descriptor->selector = selector;
    descriptor->ist = 0;
}

void interrupt_descriptor_table_t::load() {
    asm volatile("lidt %0" : : "m"(*this));
}
