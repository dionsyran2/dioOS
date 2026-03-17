/* lapic.h */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/apic/common.h>

#define LAPIC_ID_REG 0x020
#define LAPIC_EOI_REG 0x0B0


/*
ICR_HIGH (32 bits)
|-------------------|---------------------------|
| Reserved (24 bits)|   Destination ID (8 bits) |
|-------------------|---------------------------|

ICR_LOW (32 bits)
|------------------------|---------------------------------|-----------------------|---------------|-----|
| Delivery Mode (3 bits) |  Destination Shorthand (2 bits) |  Trigger Mode (1 bit) | Level (1 bit) | ... |
|------------------------|---------------------------------|-----------------------|---------------|-----|
*/
/* Registers */
#define ICR_LOW     0x300
#define ICR_HIGH    0x310

/* Delivery Mode (Bits 8-10) */
#define IPI_DELIVERY_MODE_FIXED             (0 << 8)
#define IPI_DELIVERY_MODE_LOWEST_PRIORITY   (1 << 8)
#define IPI_DELIVERY_MODE_SMI               (2 << 8)
#define IPI_DELIVERY_MODE_NMI               (4 << 8)
#define IPI_DELIVERY_MODE_INIT              (5 << 8)
#define IPI_DELIVERY_MODE_SIPI              (6 << 8)

/* Destination Mode (Bit 11) */
#define IPI_DESTINATION_MODE_PHYSICAL       (0 << 11)
#define IPI_DESTINATION_MODE_LOGICAL        (1 << 11)

/* Delivery Status (Bit 12) */
#define IPI_DELIVERY_STATUS_IDLE            (0 << 12)
#define IPI_DELIVERY_STATUS_PENDING         (1 << 12)

/* Level (Bit 14) - 0=De-assert, 1=Assert */
#define IPI_LEVEL_DEASSERT                  (0 << 14)
#define IPI_LEVEL_ASSERT                    (1 << 14) // FIX: Changed from 15 to 14

/* Trigger Mode (Bit 15) - 0=Edge, 1=Level */
#define IPI_TRIGGER_MODE_EDGE               (0 << 15)
#define IPI_TRIGGER_MODE_LEVEL              (1 << 15) // FIX: Added this definition

/* Destination Shorthand (Bits 18-19) */
#define IPI_DESTINATION_TYPE_NONE           (0 << 18)
#define IPI_DESTINATION_TYPE_SELF           (1 << 18)
#define IPI_DESTINATION_TYPE_ALL            (2 << 18)
#define IPI_DESTINATION_TYPE_OTHERS         (3 << 18)

enum ipi_destination_type_t{
    LAPIC = 0,
    SELF = 1,
    ALL = 2, // Including itself
    OTHERS = 3 // All excluding itself
};

void init_core();
void send_sipi(uint16_t apic_id, uint32_t address);
void send_init_ipi(uint16_t apic_id);
void send_ipi(uint8_t vector, uint16_t destination, ipi_destination_type_t type);

extern int local_cpu_cnt;