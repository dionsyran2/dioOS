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
    enum interrupt_type_t {
        INT_NONE = 0,
        INT_LEGACY,
        INT_MSI,
        INT_MSIX
    };
    
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

    class capability_t {
        public:
        uint8_t capability_id;
        uint8_t next_capability;
    } __attribute__ ((packed));

    class msi_capability_t : public capability_t {
        public:
        uint16_t message_control;
        uint32_t message_address_low;
        uint32_t message_address_high;
        uint16_t message_data;
        uint16_t rsv;
        uint32_t mask;
        uint32_t pending;
    } __attribute__ ((packed));

    class msix_capability_t : public capability_t {
        public:
        uint16_t message_control;    // Table size and Enable bit
        uint32_t table_offset_bir;   // Which BAR the table is in, and the offset
        uint32_t pba_offset_bir;     // Pending Bit Array
    } __attribute__((packed));

    struct msix_table_entry_t {
        uint32_t msg_address_low;  // Target CPU APIC address
        uint32_t msg_address_high; // Upper 32 bits (usually 0 for x86 APIC)
        uint32_t msg_data;         // The IDT Vector
        uint32_t vector_control;   // Bit 0 is the Mask bit (1 = Masked/Off, 0 = Unmasked/On)
    } __attribute__((packed));

    const char* get_class_name(uint8_t class_id);
    const char* get_device_name(uint16_t vendor, uint16_t device);
    const char* get_vendor_name(uint16_t vendor);
    void enumerate_pci();

    uint64_t get_device_bar(pci_device_header* device, uint8_t bar);

    void register_isr(void (handler)(void*), void *cb);
}


struct pci_device_capabilities_t {
    bool msi = false;
    bool msix = false;
};

struct pci_device_t{
    pci::pci_device_header* header;
    uint8_t bus;
    uint8_t device;
    uint8_t function;

    pci_device_capabilities_t capabilities;

    bool has_capability(uint8_t capability_id);
    pci::capability_t *get_capability(uint8_t capability_id);
    int allocate_interrupts(int requested_count);
    void register_interrupt_handler(int index, void (*handler)(void*), void* ctx, uint8_t target_cpu = 0);

    private:
    pci::interrupt_type_t irq_type = pci::INT_NONE;
    int allocated_irq_count = 0;
    
    uint8_t idt_vectors[32]; // The actual CPU IDT vectors allocated
};
