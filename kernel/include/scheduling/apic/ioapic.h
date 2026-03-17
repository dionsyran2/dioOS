/* ioapic.h */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/apic/common.h>

struct madt_header_t{
    char Signature[4]; //Should be 'APIC'
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
    uint32_t LocalAPICAddress;
    uint32_t Flags; //Flags (1 = Dual 8259 Legacy PICs Installed). If bit 0 in the flags field is set then you need to mask all the 8259 PIC's interrupts, but you should probably do this anyway.
}__attribute__ ((packed));

struct madt_entry_t{
    uint8_t Type;
    uint8_t Length;
}__attribute__ ((packed));

struct madt_io_apic_entry{ //Entry Type 1: I/O APIC
    madt_entry_t hdr;
    uint8_t APICID;
    uint8_t rsv;
    uint32_t APICAddress;
    uint32_t GSIBase; // Global System Interrupt Base
}__attribute__ ((packed));

struct madt_io_apic_iso_entry{ //Entry Type 2: I/O APIC Interrupt Source Override
    madt_entry_t hdr;
    uint8_t BusSource;
    uint8_t IRQSource;
    uint32_t GSI;
    uint16_t Flags; //See Bellow
}__attribute__ ((packed));

struct madt_io_apic_nmi_entry{
    madt_entry_t hdr;
    uint8_t nmi_source;
    uint8_t rsv;
    uint16_t flags;
    uint32_t GSI;
}__attribute__((packed));

struct madt_lapic_entry{ //Entry Type 0 | This type represents a single logical processor and its local interrupt controller.
    madt_entry_t hdr;
    uint8_t ProcessorID;
    uint8_t APICID;
    uint32_t Flags; // (bit 0 = Processor Enabled) (bit 1 = Online Capable) |
                    // If flags bit 0 is set the CPU is able to be enabled, if it is not set you need to check bit 1.
                    // If that one is set you can still enable it, if it is not the CPU can not be enabled and the OS should not try. 
}__attribute__ ((packed));

extern madt_header_t* madt_header;
void init_io_apic();
void set_apic_irq(uint8_t gsi, uint8_t vector, bool mask, bool level_sense = false, bool active_low = false, bool no_redirection = false);