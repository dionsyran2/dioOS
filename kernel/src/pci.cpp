#include "pci.h"
#include "paging/PageTableManager.h"
#include "BasicRenderer.h"
#include "cstr.h"
#include "memory/heap.h"
#include "drivers/storage/ahci/ahci.h"
#include "drivers/USB/XHCI.h"
#include <lai/helpers/sci.h>
#include <lai/helpers/pci.h>
#include <lai/core.h>
#include "scheduling/apic/apic.h"
//#include "drivers/VMWARE_SVGA2/refdriver/svga.h"
#include "drivers/audio/HDA.h"

namespace PCI{
    uint32_t cSeg = 0;
    uint32_t cBus = 0;
    uint32_t cSlot = 0;
    void EnumerateFunction(uint64_t deviceAddress, uint64_t function){
        uint64_t offset = function << 12;

        uint64_t functionAddress = deviceAddress + offset;
        globalPTM.MapMemory((void*)functionAddress, (void*)functionAddress);

        PCIDeviceHeader* pciDeviceHeader = (PCIDeviceHeader*)functionAddress;

        if (pciDeviceHeader->DeviceID == 0) return;
        if (pciDeviceHeader->DeviceID == 0xFFFF) return;

        globalRenderer->print(GetVendorName(pciDeviceHeader->VendorID));
        globalRenderer->print(" / ");
        globalRenderer->print(GetDeviceName(pciDeviceHeader->VendorID, pciDeviceHeader->DeviceID));
        globalRenderer->print(" / ");
        globalRenderer->print(DeviceClasses[pciDeviceHeader->Class]);
        globalRenderer->print(" / ");
        globalRenderer->print(GetSubclassName(pciDeviceHeader->Class, pciDeviceHeader->Subclass));
        globalRenderer->print(" / ");
        globalRenderer->print(GetProgIFName(pciDeviceHeader->Class, pciDeviceHeader->Subclass, pciDeviceHeader->ProgIF));
        globalRenderer->print("\n");
        switch (pciDeviceHeader->Class)
        {   
            case 0x06:{
                /*
                for (int i = 0; i < 4; i++){
                    acpi_resource_t *dest = (acpi_resource_t*)malloc(sizeof(acpi_resource_t));
                    lai_api_error_t error = lai_pci_route_pin(dest, cSeg, cBus, cSlot, function, i + 1);
                    if (error == LAI_ERROR_NONE){
                        //globalRenderer->printf(0x00F0F0, "_PRT %d err: %s\n", dest->base, lai_api_error_to_string(error));
                        SetIrq(dest->base, 0x50, 0);
                    }
                    free(dest);
                }*/
                break;
            }
            case 0x01: //Mass Storage Controller
                switch (pciDeviceHeader->Subclass){
                    case 0x06: //Serial ATA (SATA)
                        switch (pciDeviceHeader->ProgIF){
                            case 0x01: //AHCI 1.0 Device
                                new AHCI::AHCIDriver(pciDeviceHeader);
                            break;
                        };
                };
                break;
            case 0x0C: //Serial Bus Controller
                switch(pciDeviceHeader->Subclass){
                    case 0x03: //USB Controller
                        switch(pciDeviceHeader->ProgIF){
                            case 0x00: //UHCI
                                //usbuhci_drv = new UHCI::Driver(pciDeviceHeader);
                                break;
                            //case 0x10: //OHCI

                            case 0x20: //EHCI
                                //new EHCI::Driver(pciDeviceHeader);
                                break;
                            case 0x30: //XHCI
                                new XHCI::Driver(pciDeviceHeader);

                                break;
                        }
                }
                break;
            case 0x04: //Multimedia
                switch(pciDeviceHeader->Subclass){
                    case 0x03:{ //HD Audio
                        new HDA(pciDeviceHeader); //TODO: Make it work completely
                        break;
                    }
                }
                break;
        }   

    }

    void EnumerateDevice(uint64_t busAddress, uint64_t device){
        cSlot = device;
        uint64_t offset = device << 15;

        uint64_t deviceAddress = busAddress + offset;
        globalPTM.MapMemory((void*)deviceAddress, (void*)deviceAddress);

        PCIDeviceHeader* pciDeviceHeader = (PCIDeviceHeader*)deviceAddress;

        if (pciDeviceHeader->DeviceID == 0) return;
        if (pciDeviceHeader->DeviceID == 0xFFFF) return;

        for (uint64_t function = 0; function < 8; function++){
            EnumerateFunction(deviceAddress, function);
        }
    }

    void EnumerateBus(uint64_t baseAddress, uint64_t bus){
        cBus = bus;
        uint64_t offset = bus << 20;

        uint64_t busAddress = baseAddress + offset;
        globalPTM.MapMemory((void*)busAddress, (void*)busAddress);

        PCIDeviceHeader* pciDeviceHeader = (PCIDeviceHeader*)busAddress;

        if (pciDeviceHeader->DeviceID == 0) return;
        if (pciDeviceHeader->DeviceID == 0xFFFF) return;

        for (uint64_t device = 0; device < 32; device++){
            EnumerateDevice(busAddress, device);
        }
    }

    void EnumeratePCI(ACPI::MCFGHeader* mcfg){
        int entries = ((mcfg->Header.Length) - sizeof(ACPI::MCFGHeader)) / sizeof(ACPI::DeviceConfig);
        for (int t = 0; t < entries; t++){
            ACPI::DeviceConfig* newDeviceConfig = (ACPI::DeviceConfig*)((uint64_t)mcfg + sizeof(ACPI::MCFGHeader) + (sizeof(ACPI::DeviceConfig) * t));
            cSeg = t;
            for (uint64_t bus = newDeviceConfig->StartBus; bus < newDeviceConfig->EndBus; bus++){
                EnumerateBus(newDeviceConfig->BaseAddress, bus);
            }
        }
    }

    void* findCap(PCIDeviceHeader* dev,uint8_t id){
        PCIHeader0* hdr = (PCIHeader0*)dev;

        if (!(dev->Status & (1 << 4)))  // Bit 4 in Status register = Capabilities List
            return nullptr;
        
        uint8_t capOffset = hdr->CapabilitiesPtr;
        while (capOffset) {
            uint8_t* cap = (uint8_t*)dev + capOffset;

            if (*cap == id)
                return (void*)cap;

            capOffset = *(cap + 1);
        }
        return nullptr;
    }
}