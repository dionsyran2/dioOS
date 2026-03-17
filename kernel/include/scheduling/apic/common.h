#pragma once
#include <stdint.h>
#include <stddef.h>

#define IRQ_VECTOR_BASE 0x20
#define IOREGSEL 0x00
#define IOWIN 0x10
#define IOAPIC_ID 0x00
#define IOAPIC_REDIRECTION_BASE 0x10
#define IOAPIC_REG_VER  0x01

struct io_apic_t{
    int id;
    uint32_t base_gsi;
    uint32_t max_gsi;
    uint64_t base_address;
    io_apic_t* next;

    uint32_t read_register(uint16_t reg);
    void write_register(uint16_t reg, uint32_t val);
};

// I/O APIC Interrupt Source Override
struct io_apic_iso_t{
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
    io_apic_iso_t* next;
};

struct local_apic_t{
    int id;
    int lapic_id;

    uint64_t tick_count;
    uint64_t ticks_per_ms;

    uint64_t address;
    local_apic_t* next;

    void write_register(uint32_t reg, uint32_t data);
    uint32_t read_register(uint32_t reg);
    void EOI();
};

extern int io_apic_count;

extern local_apic_t* local_apic_list;
extern io_apic_t* io_apic_list;
extern io_apic_iso_t* io_apic_iso_list;