#include <pci.h>
#include <paging/PageTableManager.h>
#include <BasicRenderer.h>
#include <cstr.h>
#include <memory/heap.h>
#include <drivers/storage/ahci/ahci.h>
#include <drivers/USB/XHCI.h>
#include <scheduling/apic/apic.h>
#include <drivers/audio/HDA.h>
#include <uacpi/namespace.h>
#include <uacpi/uacpi.h>
#include <uacpi/internal/types.h>

ArrayList<PCI_BASE_DRIVER*>* PCI_DRIVERS;
namespace PCI{
    int parse_crs(uacpi_object* obj){
        uacpi_namespace_node *sb = uacpi_namespace_get_predefined(UACPI_PREDEFINED_NAMESPACE_SB);
        uacpi_object* link;
        char* obj_name = obj->buffer->text;

        char* str = new char[100]; //Create an absolute path to the CRS
        strcpy(str, "\\_SB_");
        if (obj_name[0] == '^'){
            strcat(str, obj->buffer->text);
            str[5] = '.';
        }else{
            strcat(str, ".");
            strcat(str, obj->buffer->text);
        }
        strcat(str, "._CRS");

        uacpi_status status = uacpi_eval(NULL, str, NULL, &link);


        if (uacpi_unlikely_error(status)) {
            kprintf(0xFF0000, "[ACPI] Failed to evaluate %s: %s\n", str, uacpi_status_to_string(status));
            return -1;
        }
        

        size_t i = 0;
        while (i < link->buffer->size) {
            uint8_t tag = link->buffer->byte_data[i];
            bool is_small = (tag & 0x80) == 0;

            if (is_small) {
                uint8_t name = (tag >> 3) & 0x0F;
                uint8_t length = tag & 0x07;
                const uint8_t *body = &link->buffer->byte_data[i + 1];

                if (name == ACPI_IRQ_FORMAT_DESCRIPTOR && length >= 2) {
                    uint8_t byte1 = body[0];
                    uint8_t byte2 = body[1];
                    uint16_t irq_bitmap = (byte2 << 8) | byte1;

                    for (int bit = 0; bit < 16; bit++) {
                        if ((irq_bitmap >> bit) & 1)
                            return bit;
                    }
                }

                i += 1 + length;

            } else {
                uint8_t name = tag & 0b01111111;
                uint16_t length = link->buffer->byte_data[i+1] | (link->buffer->byte_data[i+2] << 8);
                const uint8_t *body = &link->buffer->byte_data[i + 3];

                if (name == ACPI_EXTENDED_IRQ_DESCRIPTOR && length >= 6) {
                    uint8_t irq_flags = body[0];
                    uint8_t interrupt_table_length = body[1];
                    const uint8_t* irq = &body[2];
                    
                    return irq[0];
                }

                i += 3 + length;
            }
        }
        return -1;
    }

    uacpi_iteration_decision find_prts_callback(void *user, uacpi_namespace_node *node, uacpi_u32 depth) {
        uacpi_object_name name = uacpi_namespace_node_name(node);
        if (
            name.text[0] == '_' &&
            name.text[1] == 'P' &&
            name.text[2] == 'R' &&
            name.text[3] == 'T'
        ) {

            const uacpi_char *full_path = uacpi_namespace_node_generate_absolute_path(node);
            uacpi_object* obj;

            uacpi_status status = uacpi_eval(NULL, full_path, NULL, &obj);
            //kprintf("%s\n", full_path);
            if (uacpi_unlikely_error(status)) {
                kprintf(0xFF0000, "[ACPI] Failed to evaluate _PRT: %s\n", uacpi_status_to_string(status));
                uacpi_free_absolute_path(full_path);
                return UACPI_ITERATION_DECISION_CONTINUE;
            }

            if (obj->type != UACPI_OBJECT_PACKAGE) return UACPI_ITERATION_DECISION_CONTINUE;

            for (size_t i = 0; i < obj->package->count; i++) {

                uacpi_object *entryobj = obj->package->objects[i];
                uacpi_package* entry = entryobj->package;
                /*
                * 1st entry is the pci device
                * 2nd entry is the pci pin number
                * 3rd entry:
                *   - if its an integer, its the gsi. Find the corresponding ioapic and set it up
                *   - if its a string, its the name of another object in the namespace. You execute its _CRS method to find what
                *   interrupt it uses... it gives AN ISA IRQ??? FUCK
                */
                
                uint64_t pin_number = entry->objects[1]->integer;
                uacpi_object* gsi_obj = entry->objects[2];
                
                int gsi = 0;
                if (gsi_obj->type == UACPI_OBJECT_INTEGER){
                    gsi = gsi_obj->integer;
                }else if (gsi_obj->type == UACPI_OBJECT_STRING){
                    gsi = IRQtoGSI(parse_crs(gsi_obj));
                }

                if (gsi != -1){
                    SetIrq(gsi, PCI_INT_HANDLER_VECTOR, 0);
                }
            }

            uacpi_free_absolute_path(full_path);
        }

        uacpi_namespace_for_each_child(node, find_prts_callback, UACPI_NULL, UACPI_OBJECT_ANY_BIT, UACPI_MAX_DEPTH_ANY, UACPI_NULL);
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    void RegisterIRQs() {
        uacpi_namespace_node *sb = uacpi_namespace_get_predefined(UACPI_PREDEFINED_NAMESPACE_SB);
        uacpi_namespace_for_each_child(
            sb, find_prts_callback, UACPI_NULL,
            UACPI_OBJECT_ANY_BIT, UACPI_MAX_DEPTH_ANY, UACPI_NULL
        );
    }


    uint32_t cSeg = 0;
    uint32_t cBus = 0;
    uint32_t cSlot = 0;

    void StartDrivers(){
        for (size_t i = 0; i < PCI_DRIVERS->length(); i++){
            PCI_BASE_DRIVER* driver = PCI_DRIVERS->get(i);
            bool status = false;
            switch (driver->type){
                case PCI_DRIVER_XHCI: {
                    drivers::xhci_driver* dr = (drivers::xhci_driver*)driver;
                    status = dr->start_device();
                    break;
                }
                case PCI_DRIVER_AHCI: {
                    AHCI::AHCIDriver* dr = (AHCI::AHCIDriver*)driver;
                    status = dr->start_device();
                    break;
                }
            }

            if(!status){
                kprintf(0xFF0000, "[PCI] ");
                kprintf("Failed to start device [%h:%h]\n", driver->device_header->VendorID, driver->device_header->DeviceID);
            }
        }
    }
    void EnumerateFunction(uint64_t deviceAddress, uint64_t function){
        uint64_t offset = function << 12;

        uint64_t functionAddress = deviceAddress + offset;
        if (!globalPTM.isMapped((void*)(functionAddress & ~0xFFFUL))) globalPTM.MapMemory((void*)(functionAddress & ~0xFFFUL), (void*)deviceAddress + (offset & ~0xFFFUL));


        PCIDeviceHeader* pciDeviceHeader = (PCIDeviceHeader*)functionAddress;

        if (pciDeviceHeader->DeviceID == 0) return;
        if (pciDeviceHeader->DeviceID == 0xFFFF) return;

        /*kprintf(0x00FF00, "[PCI] ");
        kprintf("%s / %s / %s / %s / %s\n",
            GetVendorName(pciDeviceHeader->VendorID),
            GetDeviceName(pciDeviceHeader->VendorID, pciDeviceHeader->DeviceID),
            DeviceClasses[pciDeviceHeader->Class],
            GetSubclassName(pciDeviceHeader->Class, pciDeviceHeader->Subclass),
            GetProgIFName(pciDeviceHeader->Class, pciDeviceHeader->Subclass, pciDeviceHeader->ProgIF)
        );*/


        switch (pciDeviceHeader->Class)
        {   
            case 0x06:{
                
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
                        new HDA(pciDeviceHeader);
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
        if (!globalPTM.isMapped((void*)(deviceAddress & ~0xFFFUL))) globalPTM.MapMemory((void*)(deviceAddress & ~0xFFFUL), (void*)busAddress + (offset & ~0xFFFUL));

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
        if (!globalPTM.isMapped((void*)(busAddress & ~0xFFFUL))) globalPTM.MapMemory((void*)(busAddress & ~0xFFFUL), (void*)baseAddress + (offset & ~0xFFFUL));

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
                void* physical_address = (void*)newDeviceConfig->BaseAddress;
                void* virtual_address = (void*)physical_to_virtual((uint64_t)physical_address);
                globalPTM.MapMemory(virtual_address, physical_address);

                EnumerateBus((uint64_t)virtual_address, bus);
            }
        }

        RegisterIRQs();
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