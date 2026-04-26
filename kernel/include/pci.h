#pragma once
#include <stdint.h>
#include <stddef.h>
#include <acpi.h>

#define ACPI_MCFG_SIG "MCFG"
#define PCI_HEADER_TYPE_MASK 0x7F

#define PCI_CMD_IO                  (1 << 0) // I/O Space Enable
#define PCI_CMD_MEMORY              (1 << 1) // Memory Space Enable
#define PCI_CMD_BUS_MASTER          (1 << 2) // Bus Mastering Enable
#define PCI_CMD_PARRITY_ERR_RES     (1 << 6) // Parity Error Response
#define PCI_CMD_SERR                (1 << 8) // SERR# Enable
#define PCI_CMD_INTERRUPT_DISABLE   (1 << 10) // Interrupt Disable
namespace pci{
    struct pci_device_header{
        uint16_t vendor_id;
        uint16_t device_id;
        
        uint16_t command;
        uint16_t status;

        uint8_t revision_id;
        uint8_t prog_if;
        uint8_t subclass;
        uint8_t class_code;
        
        uint8_t cache_line_size;
        uint8_t latency_timer;
        uint8_t header_type;
        uint8_t BIST;
    } __attribute__ ((packed));

    struct PCIHeader0
    {
        pci_device_header Header;
        uint32_t BAR0; //Base Adress Register
        uint32_t BAR1;
        uint32_t BAR2;
        uint32_t BAR3;
        uint32_t BAR4;
        uint32_t BAR5;
        uint32_t CardbusCISPtr;
        uint16_t SubsystemVendorID;
        uint16_t SubsystemID;
        uint32_t ExpansionROMBaseAddr;
        uint8_t CapabilitiesPtr;
        uint8_t Rsv0;
        uint16_t Rsv1;
        uint32_t Rsv2;
        uint8_t InterruptLine;
        uint8_t InterruptPin;
        uint8_t MinGrant;
        uint8_t MaxLatency;
    } __attribute__ ((packed));

    struct PCIHeader1
    {
        pci_device_header Header;
        uint32_t BAR0; //Base Adress Register
        uint32_t BAR1;

        uint8_t primary_bus_number;
        uint8_t secondary_bus_number;
        uint8_t subordinate_bus_number;
        uint8_t secondary_latency_timer;

        uint8_t io_base;
        uint8_t io_limit;
        uint16_t secondary_status;

        uint16_t memory_base;
        uint16_t memory_limit;

        uint16_t prefetchable_memory_base;
        uint16_t prefetchable_memory_limit;

        uint32_t prefetchable_base_upper;

        uint32_t prefetchable_limit_upper;

        uint16_t io_base_upper;
        uint16_t io_limit_upper;

        uint8_t cap_ptr;

        uint8_t rsv[3];

        uint64_t expansion_rom_base;

        uint8_t interrupt_line;
        uint8_t interrupt_pin;
        uint16_t bridge_control;
    } __attribute__ ((packed));

    const char* get_class_name(uint8_t class_id);
    const char* get_device_name(uint16_t vendor, uint16_t device);
    const char* get_vendor_name(uint16_t vendor);
    void enumerate_pci();

    uint64_t get_device_bar(pci_device_header* device, uint8_t bar);

    void register_isr(void (handler)(void*), void *cb);
}


struct pci_device_t{
    pci::pci_device_header* header;
    uint8_t bus;
    uint8_t device;
    uint8_t function;

    pci_device_t* next;
};
