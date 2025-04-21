#pragma once
#include "../../acpi.h"
#include "../../interrupts/interrupts.h"
#include "../../math.h"


#define HPET_GLOBAL_CONF_REG                  0x010
#define HPET_GEN_INT_STS_REG                  0x020
#define HPET_MAIN_CNT_VAL_REG                 0x0F0
#define HPET_TIMER_CONF_CAP(N) (0x100 + (0x20 * N))
#define HPET_TIMER_COMP_VAL(N) (0x108 + (0x20 * N))


namespace HPET
{
    struct address_structure
    {
        uint8_t address_space_id; // 0 - system memory, 1 - system I/O
        uint8_t register_bit_width;
        uint8_t register_bit_offset;
        uint8_t reserved;
        uint64_t address;
    } __attribute__((packed));

    struct description_table_header
    {
        char signature[4]; // 'HPET' in case of HPET table
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        char oemid[6];
        uint64_t oem_tableid;
        uint32_t oem_revision;
        uint32_t creator_id;
        uint32_t creator_revision;
    } __attribute__((packed));

    struct hpet : public description_table_header
    {
        uint8_t hardware_rev_id;
        uint8_t comparator_count : 5;
        uint8_t counter_size : 1;
        uint8_t reserved : 1;
        uint8_t legacy_replacement : 1;
        uint16_t pci_vendor_id;
        address_structure address;
        uint8_t hpet_number;
        uint16_t minimum_tick;
        uint8_t page_protection;
    } __attribute__((packed));

    extern hpet *hdr;
    extern volatile uint8_t *hpet_base;
    extern uint64_t ticks;
    extern uint64_t* GCReg;
    void Initialize();
    void tick();
    void Sleep(uint64_t);
    void DisableHPET();
}