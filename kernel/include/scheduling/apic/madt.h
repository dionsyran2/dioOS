#pragma once
#include <stdint.h>
#include "../../acpi.h"
#include "../../BasicRenderer.h"
#include "../../memory/heap.h"
#include "../../paging/PageFrameAllocator.h"
struct MADTHeader{
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

//AFTER HEADER- offset 0x2c
struct Entry{
    uint8_t Type;
    uint8_t Length;
}__attribute__ ((packed));
//After the ENTRY struct | offset 2 from entry!
struct LAPIC{ //Entry Type 0 | This type represents a single logical processor and its local interrupt controller.
    uint8_t ProcessorID;
    uint8_t APICID;
    uint32_t Flags; // (bit 0 = Processor Enabled) (bit 1 = Online Capable) | If flags bit 0 is set the CPU is able to be enabled, if it is not set you need to check bit 1. If that one is set you can still enable it, if it is not the CPU can not be enabled and the OS should not try. 
}__attribute__ ((packed));
//After the ENTRY struct | offset 2 from entry!
struct IOAPIC{//Entry Type 1: I/O APIC
    uint8_t APICID;
    uint8_t rsv;
    uint32_t APICAddress;
    uint32_t GSIBase; // Global System Interrupt Base
}__attribute__ ((packed));
//After the ENTRY struct | offset 2 from entry!
struct APICISO{ //Entry Type 2: I/O APIC Interrupt Source Override
    uint8_t BusSource;
    uint8_t IRQSource;
    uint32_t GSI;
    uint16_t Flags; //See Bellow
}__attribute__ ((packed));

//After the ENTRY struct | offset 2 from entry!
struct IOAPICNMIS{ // Entry Type 3: I/O APIC Non-maskable interrupt source
    uint8_t NMISource;
    uint8_t rsv;
    uint16_t Flags; //See bellow
    uint32_t GSI;
}__attribute__ ((packed));

//After the ENTRY struct | offset 2 from entry!
//Configure these with the LINT0 and LINT1 entries in the Local vector table of the relevant processor(')s(') local APIC. 
struct LAPICNMI{ //Entry Type 4: Local APIC Non-maskable interrupts
    uint8_t ProcessorID; //0xFF means all processors
    uint16_t Flags; //See bellow
    uint8_t LINT; //0 or 1
}__attribute__ ((packed));

//After the ENTRY struct | offset 2 from entry!
/*Provides 64 bit systems with an override of the physical address of the Local APIC.
There can only be one of these defined in the MADT.
If this structure is defined,
the 64-bit Local APIC address stored within it should be used instead of the 32-bit Local APIC address stored in the MADT header.
*/
struct LAPPICAddrOverride{ //Entry Type 5: Local APIC Address Override
    uint16_t rsv;
    uint64_t LAPICAddress; //64 bit
}__attribute__ ((packed));

//After the ENTRY struct | offset 2 from entry!
/*
Represents a physical processor and its Local x2APIC.
Identical to Local APIC; used only when that struct would not be able to hold the required values.
*/
struct PLX2APIC{//Entry Type 9: Processor Local x2APIC
    uint16_t rsv;
    uint32_t LX2Address; //Processor's local x2APIC ID 
    uint32_t Flags; //Same as the Local APIC flags
    uint32_t ACPIID;
}__attribute__ ((packed));


struct ISOEntryTable{
    Entry** entries;
    uint16_t count;
};
/*
Entry type 2, 3 and 4 have a flags field, which is useful for settings up the I/O APIC redirection entry or local vector table entry respectively.
If (flags & 2) then the interrupt is active when low, and if (flags & 8) then interrupt is level-triggered.

Here is a diagram of the interrupt types:
https://wiki.osdev.org/images/9/95/Edge_vs_level.png
https://wiki.osdev.org/MADT
*/

class MADT{
    public:
    void* MADTBuffer;
    MADTHeader* MADTHdr;
    void EnumerateEntries();
    void EnumerateEntry(void* entry);
    Entry* FindIOApic();
    Entry* FindLApic(uint8_t ApicID);
    ISOEntryTable FindAPICISO();
    MADT();
};
extern MADT* madt;