/* hpet.h (High Precesion Event Timer) */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <acpi.h>
#define FEMTOSECONDS_PER_SECOND 1000000000000000ULL

#define HPET_CAPS_REG           0x00
#define HPET_GENERAL_CONF_REG                  0x010
#define HPET_GEN_INT_STS_REG                  0x020
#define HPET_MAIN_CNT_VAL_REG                 0x0F0
#define HPET_TIMER_CONF_CAP(N) (0x100 + (0x20 * N))
#define HPET_TIMER_COMP_VAL(N) (0x108 + (0x20 * N))
#define HPET_TN_INT_ROUTE_CAP_SHIFT 32
#define HPET_TN_INT_ROUTE_CNF_SHIFT 9
#define HPET_TN_INT_ROUTE_CNF_MASK  (0x1F << 9)
#define HPET_TN_INT_ENABLE          (1 << 2)

namespace HPET{
    struct hpet_t
    {
        ACPI::SDTHeader acpi_header;
        uint8_t hardware_rev_id;
        uint8_t comparator_count:5;
        uint8_t counter_size:1;
        uint8_t reserved:1;
        uint8_t legacy_replacement:1;
        uint16_t pci_vendor_id;
        ACPI::ACPI_STDAddress address;
        uint8_t hpet_number;
        uint16_t minimum_tick;
        uint8_t page_protection;
    } __attribute__((packed));
    bool initialize_hpet();
    void Sleep(uint64_t ms);
    uint64_t calibrate_tsc();
}