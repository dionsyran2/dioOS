#pragma once
#include <stdint.h>

namespace ACPI
{
    struct RSDP2
    {
        unsigned char Signature[8];
        uint8_t Checksum;
        uint8_t OEMId[6];
        uint8_t Revision;
        uint32_t RSDTAddress;
        uint32_t Length;
        uint64_t XSDTAddress;
        uint8_t ExtendedChecksum;
        uint8_t Reserved[3];
    } __attribute__((packed));

    struct SDTHeader
    {
       unsigned char Signature[4];
       uint32_t Length;
       uint8_t Revision;
       uint8_t Checksum;
       uint8_t OEMID[6];
       uint8_t OEMTableID[8];
       uint32_t OEMRevision;
       uint32_t CreatorID;
       uint32_t CreatorRevision;
    } __attribute__((packed));

    struct MCFGHeader
    {
        SDTHeader Header;
        uint64_t Reserver;
    }__attribute__((packed));
    
    struct DeviceConfig
    {
        uint64_t BaseAddress;
        uint16_t PCISegGroup;
        uint8_t StartBus;
        uint8_t EndBus;
        uint32_t Reserved;
    }__attribute__((packed));
    

    struct ACPI_GenericAddressStructure
    {
    uint8_t AddressSpace;
    uint8_t BitWidth;
    uint8_t BitOffset;
    uint8_t AccessSize;
    uint64_t Address;
    };

    struct FADT_HDR
    {
        SDTHeader hdr;
        uint32_t FirmwareCtrl;
        uint32_t Dsdt;

        // field used in ACPI 1.0; no longer in use, for compatibility only
        uint8_t  Reserved;

        uint8_t  PreferredPowerManagementProfile;
        uint16_t SCI_Interrupt;
        uint32_t SMI_CommandPort;
        uint8_t  AcpiEnable;
        uint8_t  AcpiDisable;
        uint8_t  S4BIOS_REQ;
        uint8_t  PSTATE_Control;
        uint32_t PM1aEventBlock;
        uint32_t PM1bEventBlock;
        uint32_t PM1aControlBlock;
        uint32_t PM1bControlBlock;
        uint32_t PM2ControlBlock;
        uint32_t PMTimerBlock;
        uint32_t GPE0Block;
        uint32_t GPE1Block;
        uint8_t  PM1EventLength;
        uint8_t  PM1ControlLength;
        uint8_t  PM2ControlLength;
        uint8_t  PMTimerLength;
        uint8_t  GPE0Length;
        uint8_t  GPE1Length;
        uint8_t  GPE1Base;
        uint8_t  CStateControl;
        uint16_t WorstC2Latency;
        uint16_t WorstC3Latency;
        uint16_t FlushSize;
        uint16_t FlushStride;
        uint8_t  DutyOffset;
        uint8_t  DutyWidth;
        uint8_t  DayAlarm;
        uint8_t  MonthAlarm;
        uint8_t  Century;

        // reserved in ACPI 1.0; used since ACPI 2.0+
        uint16_t BootArchitectureFlags;

        uint8_t  Reserved2;
        uint32_t Flags;

        // 12 byte structure; see below for details
        ACPI_GenericAddressStructure ResetReg;

        uint8_t  ResetValue;
        uint8_t  Reserved3[3];
    
        // 64bit pointers - Available on ACPI 2.0+
        uint64_t                X_FirmwareControl;
        uint64_t                X_Dsdt;

        ACPI_GenericAddressStructure X_PM1aEventBlock;
        ACPI_GenericAddressStructure X_PM1bEventBlock;
        ACPI_GenericAddressStructure X_PM1aControlBlock;
        ACPI_GenericAddressStructure X_PM1bControlBlock;
        ACPI_GenericAddressStructure X_PM2ControlBlock;
        ACPI_GenericAddressStructure X_PMTimerBlock;
        ACPI_GenericAddressStructure X_GPE0Block;
        ACPI_GenericAddressStructure X_GPE1Block;
    };


    
    void* FindTable(SDTHeader* sdtHeader, char* signature);
      
}

extern ACPI::SDTHeader* xsdt;
extern ACPI::MCFGHeader* mcfg;
extern ACPI::RSDP2* rsdp;