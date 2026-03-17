/* A simple acpi implementation */
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <acpica/acpi.h>
#ifdef __cplusplus
}
#endif

namespace ACPI{
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
    

    struct ACPI_STDAddress
    {
        uint8_t AddressSpace; // 0 mmio, 1 system i/o
        uint8_t BitWidth;
        uint8_t BitOffset;
        uint8_t AccessSize;
        uint64_t Address;
    }__attribute__((packed));

    void* FindTable(SDTHeader* sdtHeader, char* signature);
}

extern ACPI::RSDP2* rsdp;
extern ACPI::SDTHeader* xsdt;

void setup_acpi_kernel_events();

/* @brief If ACPI_STATUS == AE_OK then the device was found and placed in OutHandle */
ACPI_STATUS AcpiGetDevice(const char* DeviceName, ACPI_HANDLE* OutHandle);

/* @brief Attempts to enter sleep state 5 (power off) */
void power_off();


// Other stuff

void acpi_initialize_battery();
void acpi_initalize_lid();

ACPI_STATUS AcpiEvaluateInteger(ACPI_HANDLE Handle, ACPI_STRING Pathname, 
                                ACPI_OBJECT_LIST *Arguments, ACPI_INTEGER *ReturnValue);