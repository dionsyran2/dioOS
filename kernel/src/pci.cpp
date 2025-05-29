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

ArrayList<PCI_BASE_DRIVER*>* PCI_DRIVERS;
acpi_resource_t *dest = nullptr;
namespace PCI{
    uint32_t cSeg = 0;
    uint32_t cBus = 0;
    uint32_t cSlot = 0;

    void StartDrivers(){
        for (size_t i = 0; i < PCI_DRIVERS->length(); i++){
            PCI_BASE_DRIVER* driver = PCI_DRIVERS->get(i);
            if(!driver->start_device()){
                kprintf(0xFF0000, "[PCI] ");
                kprintf("Failed to start device [%h:%h]\n", driver->device_header->VendorID, driver->device_header->DeviceID);
            }
        }
    }
    void EnumerateFunction(uint64_t deviceAddress, uint64_t function){
        if (dest == nullptr) dest = new acpi_resource_t;
        uint64_t offset = function << 12;

        uint64_t functionAddress = deviceAddress + offset;
        globalPTM.MapMemory((void*)functionAddress, (void*)functionAddress);

        PCIDeviceHeader* pciDeviceHeader = (PCIDeviceHeader*)functionAddress;

        if (pciDeviceHeader->DeviceID == 0) return;
        if (pciDeviceHeader->DeviceID == 0xFFFF) return;

        kprintf(0x00FF00, "[PCI] ");
        kprintf("%s / %s / %s / %s / %s\n",
            GetVendorName(pciDeviceHeader->VendorID),
            GetDeviceName(pciDeviceHeader->VendorID, pciDeviceHeader->DeviceID),
            DeviceClasses[pciDeviceHeader->Class],
            GetSubclassName(pciDeviceHeader->Class, pciDeviceHeader->Subclass),
            GetProgIFName(pciDeviceHeader->Class, pciDeviceHeader->Subclass, pciDeviceHeader->ProgIF)
        );


        switch (pciDeviceHeader->Class)
        {   
            case 0x06:{
                for (int i = 0; i < 4; i++){
                    lai_api_error_t error = lai_pci_route_pin(dest, cSeg, cBus, cSlot, function, i + 1);
                    if (error == LAI_ERROR_NONE){
                        SetIrq(dest->base, 0x50, 0);
                    }
                }
                break;
            }
            case 0x01: //Mass Storage Controller
                switch (pciDeviceHeader->Subclass){
                    case 0x06: //Serial ATA (SATA)
                        switch (pciDeviceHeader->ProgIF){
                            case 0x01: //AHCI 1.0 Device
                                PCI_BASE_DRIVER* driver = new AHCI::AHCIDriver(pciDeviceHeader);
                                driver->driver_id = PCI_DRIVERS->length();
                                if(!driver->init_device()){
                                    kprintf(0xFF0000, "[PCI] ");
                                    kprintf("Failed to initialize driver for device [%h:%h]\n", pciDeviceHeader->VendorID, pciDeviceHeader->DeviceID);
                                    delete driver;
                                }else{
                                    PCI_DRIVERS->add(driver);
                                }
                            break;
                        };
                };
                break;
            case 0x0C: //Serial Bus Controller
                switch(pciDeviceHeader->Subclass){
                    case 0x03: //USB Controller
                        switch(pciDeviceHeader->ProgIF){
                            case 0x00: //UHCI
                                break;
                            //case 0x10: //OHCI

                            case 0x20: //EHCI
                                break;

                            case 0x30: //XHCI
                                drivers::xhci_driver* driver = new drivers::xhci_driver(pciDeviceHeader);
                                driver->driver_id = PCI_DRIVERS->length();

                                if(!driver->init_device()){
                                    kprintf(0xFF0000, "[PCI] ");
                                    kprintf("Failed to initialize driver for device [%h:%h]\n", pciDeviceHeader->VendorID, pciDeviceHeader->DeviceID);
                                    delete driver;
                                }else{
                                    PCI_DRIVERS->add(driver);
                                }

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
        PCI_DRIVERS = new ArrayList<PCI_BASE_DRIVER*>();

        int entries = ((mcfg->Header.Length) - sizeof(ACPI::MCFGHeader)) / sizeof(ACPI::DeviceConfig);
        for (int t = 0; t < entries; t++){
            ACPI::DeviceConfig* newDeviceConfig = (ACPI::DeviceConfig*)((uint64_t)mcfg + sizeof(ACPI::MCFGHeader) + (sizeof(ACPI::DeviceConfig) * t));
            cSeg = t;
            for (uint64_t bus = newDeviceConfig->StartBus; bus < newDeviceConfig->EndBus; bus++){
                EnumerateBus(newDeviceConfig->BaseAddress, bus);
            }
        }

        StartDrivers();
    }

    void* findCap(PCIDeviceHeader* dev, uint8_t id){
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

PCI_BASE_DRIVER::PCI_BASE_DRIVER(PCI::PCIDeviceHeader* hdr){
    this->device_header = hdr;
}

PCI_BASE_DRIVER::~PCI_BASE_DRIVER(){

}