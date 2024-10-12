#include "USB.h"
#include "../pci.h"
#include "../BasicRenderer.h"
#include "../cstr.h"
#include "../scheduling/pit/pit.h"
#include "../memory/heap.h"

QueueHead* EHCIDriver::GetQueueHeadForEndpoint(uint8_t port, uint8_t endpoint) {
    // Check if the port and endpoint are valid
    if (port < MAX_USB_PORTS_EHCI && endpoint < MAX_USB_ENDPOINTS) {
        return queueHeads[port][endpoint];
    }
    return nullptr; // Invalid port/endpoint
}

void EHCIDriver::WriteToEndpoint(uint8_t port, uint8_t endpoint, void* data, size_t length) {
    TransferDescriptor* td = new TransferDescriptor();
    td->Status = 0;
    td->Token = (USB_ENDPOINT_DIRECTION_OUT | (length & 0x7FF)); // Set direction and length
    td->BufferPointer = (uint64_t)data; // Set buffer pointer
    td->NextTD = 0;

    QueueHead* qh = GetQueueHeadForEndpoint(port, endpoint);
    qh->CurrentTD = (uint64_t)td;

    // Start the transfer by setting the appropriate bits in the EHCI registers
    volatile uint32_t* commandReg = (volatile uint32_t*)((uint64_t)EHCIBaseAddress + 0x00);
    *commandReg |= (1 << 0); // Set the Run/Stop bit to run the controller
}

void EHCIDriver::ReadFromEndpoint(uint8_t port, uint8_t endpoint, void* data, size_t length) {
    // Create a new transfer descriptor
    TransferDescriptor* td = new TransferDescriptor();
    td->Status = 0; // Initialize status
    td->Token = (USB_ENDPOINT_DIRECTION_IN | (length & 0x7FF)); // Set direction and length
    td->BufferPointer = (uint64_t)data; // Set buffer pointer
    td->NextTD = 0;


    QueueHead* qh = GetQueueHeadForEndpoint(port, endpoint);
    qh->CurrentTD = (uint64_t)td; // Point to our transfer descriptor

    // Start the transfer
    volatile uint32_t* commandReg = (volatile uint32_t*)((uintptr_t)EHCIBaseAddress + 0x00);
    *commandReg |= (1 << 0); // Set the Run/Stop bit to run the controller
}

bool EHCIDriver::IsTransferComplete(uint8_t port, uint8_t endpoint) {
    // Calculate the address of the Queue Head for the specified port and endpoint
    QueueHead* queueHead = GetQueueHeadForEndpoint(port, endpoint);
    
    if (!queueHead) {
        return false; // Queue Head not found, transfer cannot be checked
    }

    // Read the status of the Queue Head
    uint32_t control = queueHead->Control;

    if (control & (1 << 30)) {
        return true; // Transfer is complete
    }
    
    // The transfer is still in progress
    return false;
}

bool EHCIDriver::SendControlTransfer(uint8_t port, USBSetupPacket* setupPacket, void* data, size_t length) {
    // Set the appropriate endpoint for control transfers (usually endpoint 0)
    uint8_t endpoint = 0;

    WriteToEndpoint(port, endpoint, setupPacket, sizeof(USBSetupPacket));

    while (!IsTransferComplete(port, endpoint));

    ReadFromEndpoint(port, endpoint, data, length);

    return true;
}


USBDeviceDescriptor EHCIDriver::GetDeviceDescriptor(uint8_t port){
    USBDeviceDescriptor usbDeviceDescriptor;

    USBSetupPacket setupPacket;
    setupPacket.requestType = 0x80; //Device To Host
    setupPacket.request = USB_REQUEST_GET_DESCRIPTOR; //Set the request to get the device descriptor
    setupPacket.value = (USB_DESCRIPTOR_TYPE_DEVICE << 8); //Descriptor Type
    setupPacket.index = 0x00;
    setupPacket.length = sizeof(usbDeviceDescriptor);
    SendControlTransfer(port, &setupPacket, &usbDeviceDescriptor, sizeof(usbDeviceDescriptor));


    globalRenderer->print("Device Vendor ID: ");
    globalRenderer->print(toString(static_cast<int64_t>(usbDeviceDescriptor.VendorID)));
    globalRenderer->print("/Device Product ID: ");
    globalRenderer->print(toString(static_cast<int64_t>(usbDeviceDescriptor.ProductID)));
    globalRenderer->print("/Device Class: ");
    globalRenderer->print(toString(static_cast<int64_t>(usbDeviceDescriptor.DeviceClass)));
    globalRenderer->print("\n");
    return usbDeviceDescriptor;
}

void EHCIDriver::ResetEHCI(void* EHCIBaseAddress){
    volatile uint32_t* cmd_reg = (volatile uint32_t*)(((uint64_t)EHCIBaseAddress) + 0x00); //USB CMD register offset
    *cmd_reg |= (1 << 1); // Set the Reset bit in USBCMD

    while (*cmd_reg & (1 << 1)); //When the reset is complete, the bit will reset back to 0
}

void* EHCIDriver::AllocateFrameListMemory(){
    void* memory = GlobalAllocator.RequestPage();
    memset(memory, 0, 4096);
    return memory;
}

void EHCIDriver::InitializeFrameList(void* EHCIBaseAddress){
    void* frameList = AllocateFrameListMemory();

    volatile uint32_t* frameListPtr = (volatile uint32_t*)((uintptr_t)EHCIBaseAddress + 0x20); // Frame List Base Address Register
    *frameListPtr = ((uintptr_t)frameList & ~0x3) | 0x1; // Set the address and mark it as active
}

void EHCIDriver::EnableInterrupts(void* EHCIBaseAddress){
    volatile uint32_t* usbCmd = (volatile uint32_t*)((uintptr_t)EHCIBaseAddress + 0x00); // USB Command register
    volatile uint32_t* usbSts = (volatile uint32_t*)((uintptr_t)EHCIBaseAddress + 0x04); // USB Status register
    *usbCmd |= (1 << 0); // Set the Run/Stop bit
    *usbSts |= (1 << 0); // Enable USB interrupts
}

void EHCIDriver::ConfigurePorts(void* EHCIBaseAddress) {
    volatile uint32_t* portStatus = (volatile uint32_t*)((uintptr_t)EHCIBaseAddress + 0x40); // Port Status and Control register

    // Check for the number of ports (usually in EHCI spec)
    for (int i = 0; i < MAX_USB_PORTS_EHCI; ++i) {
        uint32_t portReg = *(portStatus + i);
        
        // Reset the port
        portReg |= (1 << 1); // Set the Port Reset bit
        int timeout = 10000; // Set a timeout limit
        while ((*(portStatus + i) & (1 << 1)) && timeout) {
            timeout--; // Decrement the timeout counter
        }

        if (timeout <= 0) {
            globalRenderer->print("Timeout waiting for port reset to complete on port ");
            globalRenderer->print(toString(static_cast<int64_t>(i)));
            globalRenderer->print("\n");
            continue; // Skip to the next port if we timed out
        }
        
        // Power on the port
        portReg |= (1 << 0); // Set the Port Power bit
        *(portStatus + i) = portReg; // Write back the modified port status

        //Check the connection status
        if (portReg & (1 << 0)) {
            globalRenderer->print("Device connected on port");
            globalRenderer->print(toString(static_cast<int64_t>(i)));
            globalRenderer->print("\n");
            // Handle device initialization and enumeration
        }
    }
}

void EHCIDriver::EnumerateDevices(void* EHCIBaseAddress) {
    for (uint8_t i = 0; i < MAX_USB_PORTS_EHCI; ++i) {
        volatile uint32_t* portStatus = (volatile uint32_t*)((uintptr_t)EHCIBaseAddress + 0x40 + (i * 4)); // Port Status register

        uint32_t status = *portStatus;
        if (status & (1 << 0)) { // Check if a device is connected
            globalRenderer->print("Device detected on port ");
            globalRenderer->print(toString(static_cast<int64_t>(i)));
            globalRenderer->print("\n");

            // Perform the necessary steps to reset the device and retrieve its descriptors
            // Reset the device
            globalRenderer->print("Resetting Device...\n");
            *portStatus |= (1 << 1); // Set the Port Reset bit
            while (status & (1 << 1));
            globalRenderer->print("Reset Complete!\n");
            GetDeviceDescriptor(i);
        }
    }
}



void EHCIDriver::StartController(void* EHCIBaseAddress) {
    volatile uint32_t* cmd_reg = (volatile uint32_t*)((uintptr_t)EHCIBaseAddress + 0x00);
    *cmd_reg |= 1; // Set the Run/Stop bit to run the controller
}



EHCIDriver::EHCIDriver(PCI::PCIDeviceHeader* pciDeviceHeader){
    globalRenderer->delay(5000);
    PCI::PCIHeader0* EHCIHdr = (PCI::PCIHeader0*)pciDeviceHeader;
    void* EHCIBaseAddress_ = (void*)((uintptr_t)EHCIHdr->BAR0 & ~0xF); // Mask out the lower bits
    EHCIBaseAddress = EHCIBaseAddress_;
    if (EHCIBaseAddress == 0) return;//Invalid BAR address

    globalPTM.MapMemory(EHCIBaseAddress, EHCIBaseAddress);

    memset(queueHeads, 0, sizeof(queueHeads));

    globalRenderer->delay(5000);

    globalRenderer->Clear(0x0f0024);
    globalRenderer->cursorPos.X = 0;
    globalRenderer->cursorPos.Y = 0;
    ResetEHCI(EHCIBaseAddress);
    InitializeFrameList(EHCIBaseAddress);
    EnableInterrupts(EHCIBaseAddress);
    InitializeQueueHeads(EHCIBaseAddress);
    ConfigurePorts(EHCIBaseAddress);
    StartController(EHCIBaseAddress);
    EnumerateDevices(EHCIBaseAddress);


    globalRenderer->print("EHCI Initialized Successfully!\n");
}

void EHCIDriver::InitializeQueueHeads(void* EHCIBaseAddress) {
    for (int port = 0; port < MAX_USB_PORTS_EHCI; ++port) {
        for (int endpoint = 0; endpoint < MAX_USB_ENDPOINTS; ++endpoint) {
            // Allocate memory for each QueueHead
            queueHeads[port][endpoint] = (QueueHead*)GlobalAllocator.RequestPage();

            // Ensure memory allocation was successful
            if (queueHeads[port][endpoint] == nullptr) {
                // Handle the error: allocation failed
                globalRenderer->print("Failed to allocate memory for QueueHead\n");
                continue; // Skip to the next iteration
            }

            memset(queueHeads[port][endpoint], 0, sizeof(QueueHead));

            queueHeads[port][endpoint]->HorizontalLink = 0;
            queueHeads[port][endpoint]->EndpointCaps = 0;
            queueHeads[port][endpoint]->CurrentTD = 0;
            queueHeads[port][endpoint]->Control = 0;
        }
    }
}

EHCIDriver::~EHCIDriver() {
    //DELETE HERE, MEMORY LEAK!!!
}