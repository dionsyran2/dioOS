#include <pci.h>
#include <kstdio.h>
#include <memory.h>
#include <paging/PageTableManager.h>
#include <filesystem/vfs/vfs.h>
#include <cstr.h>
#include <CONFIG.h>
#include <drivers/drivers.h>


namespace pci{
    pci_device_t* pci_device_descriptor_list = nullptr;

    void print_device_info(pci_device_header* header){
        kprintf("\e[0;35m[PCI]\e[0m %s / %s / %s / %.2X / %.2X\n",
            get_vendor_name(header->vendor_id),
            get_device_name(header->vendor_id, header->device_id),
            get_class_name(header->class_code),
            header->subclass,
            header->prog_if
        );
    }

    void _find_driver(pci_device_header* device){
        for (driver_class_t* driver = __start_drivers; driver < __stop_drivers; driver++){
            if (driver->supports_device(device)){
                base_driver_t* drv = driver->create_instance(device);
                if (drv->init_device()){
                    add_driver_to_list(drv);
                }else{
                    kprintf("\e[0;31m[DRIVERS]\e[0m driver %s for device %.4x:%.4x failed to initialize!\n",
                        driver->name, device->vendor_id, device->device_id);
                    delete drv;
                }
            }
        }
    }

    uint64_t calculate_address(uint64_t mcfg_base, uint8_t bus, uint8_t device, uint8_t function) {
        // 1 Bus = 1MB (20 bits), 1 Device = 32KB (15 bits), 1 Function = 4KB (12 bits)
        return mcfg_base + ((uint64_t)bus << 20) + ((uint64_t)device << 15) + ((uint64_t)function << 12);
    }

    // Forward declaration
    void enumerate_bus(uint64_t base_address, uint8_t bus);

    void create_device_descriptor(pci_device_header* header, uint8_t bus, uint8_t device, uint8_t function){
        pci_device_t* desc = new pci_device_t();
        desc->bus = bus;
        desc->device = device;
        desc->function = function;
        desc->header = header;
        desc->next = nullptr;

        if (pci_device_descriptor_list == nullptr){
            pci_device_descriptor_list = desc;
            return;
        }

        pci_device_t* c = pci_device_descriptor_list;
        while(c->next) c = c->next;

        c->next = desc;
    }

    void enumerate_function(uint64_t mcfg_base, uint8_t bus, uint8_t device, uint8_t function) {
        uint64_t physical = calculate_address(mcfg_base, bus, device, function);
        uint64_t virt = physical_to_virtual(physical);

        if (globalPTM.getPhysicalAddress((void*)(virt & (~0xFFF))) == 0){
            globalPTM.MapMemory((void*)(virt & (~0xFFF)), (void*)(physical & (~0xFFF)));
            globalPTM.SetFlag((void*)(virt & (~0xFFF)), PT_Flag::CacheDisable, true);
        }

        pci_device_header* header = (pci_device_header*)virt;
        if (header->device_id == 0 || header->device_id == 0xFFFF) return;

        #ifdef LOG_PCI_DEVICE_ENUMERATION
        print_device_info(header);
        #endif

        #ifdef EXPOSE_PCI_DEVICES_IN_VFS
        create_device_descriptor(header, bus, device, function);
        #endif
        
        if ((header->header_type & PCI_HEADER_TYPE_MASK) == 0x01){ // PCI-PCI bridge
            PCIHeader1* bridge = (PCIHeader1*)header;
            enumerate_bus(mcfg_base, bridge->secondary_bus_number);
        }

        _find_driver(header);
    }

    void enumerate_device(uint64_t mcfg_base, uint8_t bus, uint8_t device) {
        // We need to peek at Function 0 to know if this is a Multi-Function device
        uint64_t physical = calculate_address(mcfg_base, bus, device, 0);
        uint64_t virt = physical_to_virtual(physical);

        if (globalPTM.getPhysicalAddress((void*)(virt & (~0xFFF))) == 0){
            globalPTM.MapMemory((void*)(virt & (~0xFFF)), (void*)(physical & (~0xFFF)));
            globalPTM.SetFlag((void*)(virt & (~0xFFF)), PT_Flag::CacheDisable, true);
        }

        pci_device_header* header = (pci_device_header*)virt;
        if (header->device_id == 0 || header->device_id == 0xFFFF) return;

        uint8_t function_count = 1;
        if (header->header_type & 0x80) {
            function_count = 8;
        }

        for (int f = 0; f < function_count; f++) {
            enumerate_function(mcfg_base, bus, device, f);
        }
    }

    void enumerate_bus(uint64_t base_address, uint8_t bus){
        for (int i = 0; i < 32; i++) {
            enumerate_device(base_address, bus, i);
        }
    }

    void create_pci_vfs_entries();

    void enumerate_pci(){
        ACPI_TABLE_HEADER *mcfg = NULL;
        ACPI_STATUS status = AcpiGetTable(ACPI_MCFG_SIG, 1, &mcfg);

        if (ACPI_FAILURE(status)){
            kprintf("[PCI] Could not get the MCFG ACPI Table: %s\n", AcpiFormatException(status));
            return;
        }

        ACPI_TABLE_MCFG *mcfg_full = (ACPI_TABLE_MCFG *)mcfg;
        ACPI_MCFG_ALLOCATION *alloc = (ACPI_MCFG_ALLOCATION *)(mcfg_full + 1);

        UINT32 data_length = mcfg->Length - sizeof(ACPI_TABLE_MCFG);
        int num_entries = data_length / sizeof(ACPI_MCFG_ALLOCATION);

        for (int i = 0; i < num_entries; i++) {
            enumerate_bus(alloc[i].Address, 0);
        }

        create_pci_vfs_entries();
    }

    void create_pci_vfs_entries(){
        vnode_t* bus = vfs::resolve_path("/proc/bus");
        if (!bus) return;

        bus->mkdir("pci");
        bus->close();

        vnode_t* pci = vfs::resolve_path("/proc/bus/pci");
        
        if (!pci) return kprintf("[PCI] Could not create '/proc/bus/pci'");

        char name_buffer[64];
        char path_buffer[128];
        for (pci_device_t* device = pci_device_descriptor_list; device != nullptr; device = device->next){
            // calculate the path
            stringf(path_buffer, sizeof(path_buffer), "/proc/bus/pci/%.2x", device->bus);
            vnode_t* bus = vfs::resolve_path(path_buffer);

            // If it does not exist, create it
            if (!bus){
                stringf(name_buffer, sizeof(name_buffer), "%.2x", device->bus);
                pci->mkdir(name_buffer);

                bus = vfs::resolve_path(path_buffer);
            }

            if (!bus) continue;

            // Calculate the device path
            stringf(path_buffer, sizeof(path_buffer), "/proc/bus/pci/%.2x/%.2x.%.1x", device->bus, device->device, device->function);

            vnode_t* dev = vfs::resolve_path(path_buffer);

            // If it does not exist, create it
            if (!dev){
                stringf(name_buffer, sizeof(name_buffer), "%.2x.%.1x", device->device, device->function);
                bus->creat(name_buffer);

                dev = vfs::resolve_path(path_buffer);
            }

            if (!dev) continue;

            // Create a copy of the config space (Should be a window but I am too lazy to do that now)
            dev->write(0, sizeof(pci_device_header), device->header);
        }
    }

    uint32_t* _get_bar(pci_device_header* device, uint8_t bar){
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Waddress-of-packed-member"

        PCIHeader0* hdr = (PCIHeader0*)device;
        switch (bar){
            case 0: return &hdr->BAR0;
            case 1: return &hdr->BAR1;
            case 2: return &hdr->BAR2;
            case 3: return &hdr->BAR3;
            case 4: return &hdr->BAR4;
            case 5: return &hdr->BAR5;
        }

        return nullptr;

        #pragma GCC diagnostic pop
    }

    uint32_t _get_bar_size(pci_device_header* device, uint8_t idx){
        uint32_t* bar = _get_bar(device, idx);
        if (bar == nullptr) return 0;

        uint32_t original_bar = *bar;

        *bar = 0xFFFFFFFF;

        uint32_t mask = *bar;

        *bar = original_bar;

        mask &= 0xFFFFFFF0;

        // Compute size as the lowest bit set + 1 trick
        uint32_t bar_size = (~mask) + 1;
        return bar_size;
    }

    uint64_t get_device_bar(pci_device_header* device, uint8_t bar){
        if (bar > 5) return 0;

        uint32_t* pbar0 = _get_bar(device, bar);
        uint32_t* pbar1 = _get_bar(device, bar + 1);
        uint32_t bar0 = pbar0 != nullptr ? *pbar0 : 0;

        if (bar0 & 1) return bar0 & ~(0b11U); // IO address

        uint32_t bar1 = pbar1 != nullptr ? *pbar1 : 0;
        uint32_t bar_size = _get_bar_size(device, bar);

        uint64_t physical_address = bar0 & 0xFFFFFFF0;

        if ((bar0 & 0b110) == 0b100){
            physical_address += ((uint64_t)bar1) << 32;
        }

        uint64_t virtual_address = get_virtual_device_mmio_address(physical_address);

        for (uint32_t i = 0; i < bar_size; i += 0x1000){
            globalPTM.MapMemory((void*)(virtual_address + i), (void*)(physical_address + i));
            globalPTM.SetFlag((void*)(virtual_address + i), PT_Flag::CacheDisable, true);
        }

        return virtual_address;
    }
}
