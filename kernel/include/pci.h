#pragma once
#include <stdint.h>
#include "acpi.h"
#include "ArrayList.h"


#define PCI_CAP_MSIX_ID 0x11
#define ACPI_IRQ_FORMAT_DESCRIPTOR 0x04
#define ACPI_EXTENDED_IRQ_DESCRIPTOR 0x09
#define PCI_INT_HANDLER_VECTOR 0x50

#define PCI_DRIVER_AHCI 0x01
#define PCI_DRIVER_XHCI 0x02

namespace PCI{
    struct PCIDeviceHeader{
        uint16_t VendorID;
        uint16_t DeviceID;
        uint16_t Command;
        uint16_t Status;
        uint8_t RevisionID;
        uint8_t ProgIF;
        uint8_t Subclass;
        uint8_t Class;
        uint8_t CacheLineSize;
        uint8_t LatencyTimer;
        uint8_t HeaderType;
        uint8_t BIST;
    };

    struct PCIHeader0
    {
        PCIDeviceHeader Header;
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
    };

    struct MSIX_MESG_CTRL{
        uint16_t tableSize:11;
        uint8_t rsv:3;
        uint8_t functionMask:1;
        uint8_t enable:1;
    }__attribute__((packed));

    struct MSIX_Capability {
        uint8_t  CapID;        // Offset +0: Capability ID (should be 0x11)
        uint8_t  NextPtr;      // Offset +1: Pointer to next capability
        MSIX_MESG_CTRL MsgCtrl;      // Offset +2: Message Control (enables MSI-X)
        uint32_t TableOffset;  // Offset +4: BAR and offset of MSI-X Table
        uint32_t PBAOffset;    // Offset +8: BAR and offset of Pending Bit Array
    } __attribute__((packed));

    struct MSIX_TableEntry{
        uint32_t MsgAddressLo;
        uint32_t MsgAddressHi;
        uint32_t MsgData;
        uint32_t VectorCtrl;
    }__attribute__((packed));

    

    void EnumeratePCI(ACPI::MCFGHeader* mcfg);

    extern const char* DeviceClasses[];

    const char* GetVendorName(uint16_t vendorID);
    const char* GetDeviceName(uint16_t vendorID, uint16_t deviceID);
    const char* GetSubclassName(uint8_t classCode, uint8_t subclassCode);
    const char* GetProgIFName(uint8_t classCode, uint8_t subclassCode, uint8_t progIF);
    void* findCap(PCIDeviceHeader* dev,uint8_t id);
}

class PCI_BASE_DRIVER{
    public:
    int type;
    PCI::PCIDeviceHeader* device_header;
    int driver_id;
    PCI_BASE_DRIVER(PCI::PCIDeviceHeader* hdr);
    ~PCI_BASE_DRIVER();
    virtual bool init_device() = 0;
    virtual bool start_device() = 0;
    virtual bool shutdown_device() = 0;
};


extern ArrayList<PCI_BASE_DRIVER*>* PCI_DRIVERS;
