#pragma once
#include "../pci.h"
#include <stdint.h>
#include "../paging/PageFrameAllocator.h"
#include "../paging/PageTableManager.h"

#define MAX_USB_PORTS_EHCI 16
#define MAX_USB_ENDPOINTS 16
#define USB_REQUEST_GET_DESCRIPTOR 0x06
#define USB_DESCRIPTOR_TYPE_DEVICE 0x01
#define USB_ENDPOINT_DIRECTION_OUT 0x00
#define USB_ENDPOINT_DIRECTION_IN  0x80 

struct TransferDescriptor {
    uint32_t Status;     // Status of the transfer
    uint32_t Token;      // Token (address, endpoint, data toggle, etc.)
    uint32_t BufferPointer; // Pointer to data buffer
    uint32_t NextTD;     // Pointer to next transfer descriptor
};

struct QueueHead {
    uint32_t HorizontalLink; // Next queue head
    uint32_t EndpointCaps;   // Endpoint capabilities
    uint32_t CurrentTD;       // Current transfer descriptor
    uint32_t Control;         // Control for the endpoint
};


struct USBDeviceDescriptor {
    uint8_t Length;
    uint8_t DescriptorType;
    uint16_t USBVersion;
    uint8_t DeviceClass;
    uint8_t DeviceSubClass;
    uint8_t DeviceProtocol;
    uint8_t MaxPacketSize0;
    uint16_t VendorID;
    uint16_t ProductID;
    uint16_t DeviceReleaseNumber;
    uint8_t ManufacturerIndex;
    uint8_t ProductIndex;
    uint8_t SerialNumberIndex;
    uint8_t NumberOfConfigurations;
};

struct USBSetupPacket{
    uint8_t requestType;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
};

class EHCIDriver{
    public:
    EHCIDriver(PCI::PCIDeviceHeader* pciDeviceHeader);
    ~EHCIDriver();
    void ResetEHCI(void* EHCIBaseAddress);
    void InitializeQueueHeads(void* EHCIBaseAddress);
    private:
        void* EHCIBaseAddress;
        bool IsTransferComplete(uint8_t port, uint8_t endpoint);
        QueueHead* queueHeads[MAX_USB_PORTS_EHCI][MAX_USB_ENDPOINTS];
        QueueHead* GetQueueHeadForEndpoint(uint8_t port, uint8_t endpoint);
        void WriteToEndpoint(uint8_t port, uint8_t endpoint, void* data, size_t length);
        void ReadFromEndpoint(uint8_t port, uint8_t endpoint, void* data, size_t length);
        bool SendControlTransfer(uint8_t port, USBSetupPacket* setupPacket, void* data, size_t length);
        USBDeviceDescriptor GetDeviceDescriptor(uint8_t port);
        void* AllocateFrameListMemory();
        void InitializeFrameList(void* EHCIBaseAddress);
        void EnableInterrupts(void* EHCIBaseAddress);
        void ConfigurePorts(void* EHCIBaseAddress);
        void EnumerateDevices(void* EHCIBaseAddress);
        void StartController(void* EHCIBaseAddress);
};