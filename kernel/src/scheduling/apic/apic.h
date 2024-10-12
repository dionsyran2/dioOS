#pragma once
#include <stdint.h>
#include "../../cpu.h"
#include "../../BasicRenderer.h"
#include "../../paging/PageTableManager.h"
#include "../../paging/PageFrameAllocator.h"
#include "../../memory.h"

#ifndef APIC_H
#define APIC_H

// APIC Base Address
#define APIC_BASE_ADDRESS 0xFEE00000 // Common base address for APIC

// APIC Register Offsets
#define APIC_SPURIOUS_INT_VECTOR 0xF0 // Spurious Interrupt Vector Register offset
#define ICR_LOW_OFFSET 0x300          // ICR low register offset
#define ICR_HIGH_OFFSET 0x310         // ICR high register offset
#define APIC_ID_OFFSET 0x20           // APIC ID offset
#define APIC_BASE_MSR 0x1B
struct MADTHeader {
    char signature[4]; // "APIC"
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t oem_table_id[8];
    uint32_t oem_table_version;
    uint32_t rsdt_address;
    uint32_t length2; // Length of this structure
    uint64_t xsdt_address; // 64-bit address of the Extended System Description Table
    uint8_t reserved[8]; // Reserved
};
enum MADTEntryType {
    LocalAPIC = 0,
    IOAPIC = 1,
    InterruptSourceOverride = 2,
    NMI = 3,
    LocalAPICAddressOverride = 4,
    IOAPICSourceOverride = 5,
    Localx2APIC = 9,
};
struct LocalAPICEntry {
    uint8_t type;           // Entry type (0 for Local APIC)
    uint8_t length;         // Length of the structure
    uint8_t acpi_id;        // ACPI processor ID (local APIC ID)
    uint8_t apic_id;        // APIC ID
    uint32_t flags;         // Flags (e.g., enabled or disabled)
};
struct ProcessorInfo {
    uint32_t apicID;         // Processor ID
    uintptr_t mailboxAddr;   // Mailbox physical address
    // Add more fields as needed (e.g., state, type)
};
// APIC Configuration Functions
void InitLocalAPIC();
void SendIPI(uint8_t vector, uint8_t destinationAPICID);
void InitAllCPUs();
void SendEndOfInterrupt();


#endif // APIC_H
