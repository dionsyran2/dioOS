#include <pci.h>
#include <kstdio.h>
#include <memory.h>
#include <paging/PageTableManager.h>
#include <cstr.h>
#include <CONFIG.h>
#include <drivers/drivers.h>
#include <interrupts/interrupts.h>
#include <scheduling/apic/ioapic.h>
#include <acpi.h>
#include <local.h>
#include <scheduling/apic/lapic.h>


void init_pci_interrupt_routing();

namespace pci{
    void print_device_info(pci_device_header* header){
        kprintf("\e[0;35m[PCI]\e[0m %s / %s / %s / %.2X / %.2X\n",
            get_vendor_name(header->vendor_id),
            get_device_name(header->vendor_id, header->device_id),
            get_class_name(header->class_code),
            header->subclass,
            header->prog_if
        );
    }

    bool _find_driver(pci_device_t* device){
        for (driver_class_t* driver = __start_drivers; driver < __stop_drivers; driver++){
            if (driver->supports_device(device)){
                base_driver_t* drv = driver->create_instance(device);
                if (drv->init_device()){
                    add_driver_to_list(drv);
                    return true;
                }else{
                    kprintf("\e[0;31m[DRIVERS]\e[0m driver %s for device %.4x:%.4x failed to initialize!\n",
                        driver->name, device->header->vendor_id, device->header->device_id);
                    delete drv;
                }
            }
        }

        return false;
    }

    uint64_t calculate_address(uint64_t mcfg_base, uint8_t bus, uint8_t device, uint8_t function) {
        // 1 Bus = 1MB (20 bits), 1 Device = 32KB (15 bits), 1 Function = 4KB (12 bits)
        return mcfg_base + ((uint64_t)bus << 20) + ((uint64_t)device << 15) + ((uint64_t)function << 12);
    }

    // Forward declaration
    void enumerate_bus(uint64_t base_address, uint8_t bus);

    pci_device_t *create_device_descriptor(pci_device_header* header, uint8_t bus, uint8_t device, uint8_t function){
        pci_device_t* desc = new pci_device_t();
        desc->bus = bus;
        desc->device = device;
        desc->function = function;
        desc->header = header;
        return desc;
    }

    void enumerate_function(uint64_t mcfg_base, uint8_t bus, uint8_t device, uint8_t function) {
        uint64_t physical = calculate_address(mcfg_base, bus, device, function);
        uint64_t virt = physical_to_virtual(physical);

        if (globalPTM.getPhysicalAddress((void*)(virt & (~0xFFF))) == 0){
            globalPTM.MapMemory((void*)(virt & (~0xFFF)), (void*)(physical & (~0xFFF)), (1UL << Write));
            globalPTM.SetFlag((void*)(virt & (~0xFFF)), PT_Flag::CacheDisable, true);
        }

        pci_device_header* header = (pci_device_header*)virt;
        if (header->device_id == 0 || header->device_id == 0xFFFF) return;

        #ifdef LOG_PCI_DEVICE_ENUMERATION
        print_device_info(header);
        #endif
        
        if ((header->header_type & PCI_HEADER_TYPE_MASK) == 0x01){ // PCI-PCI bridge
            PCIHeader1* bridge = (PCIHeader1*)header;
            enumerate_bus(mcfg_base, bridge->secondary_bus_number);
        }

        pci_device_t *dev = create_device_descriptor(header, bus, device, function);
        dev->capabilities.msi = dev->has_capability(0x05);
        dev->capabilities.msix = dev->has_capability(0x11);
        if (!_find_driver(dev)) {
            delete dev;
        }
    }

    void enumerate_device(uint64_t mcfg_base, uint8_t bus, uint8_t device) {
        // We need to peek at Function 0 to know if this is a Multi-Function device
        uint64_t physical = calculate_address(mcfg_base, bus, device, 0);
        uint64_t virt = physical_to_virtual(physical);

        if (globalPTM.getPhysicalAddress((void*)(virt & (~0xFFF))) == 0){
            globalPTM.MapMemory((void*)(virt & (~0xFFF)), (void*)(physical & (~0xFFF)), (1UL << Write));
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

        //create_pci_vfs_entries();
        init_pci_interrupt_routing();
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
            globalPTM.MapMemory((void*)(virtual_address + i), (void*)(physical_address + i), (1UL << Write));
            globalPTM.SetFlag((void*)(virtual_address + i), PT_Flag::CacheDisable, true);
            globalPTM.SetFlag((void*)(virtual_address + i), PT_Flag::WriteThrough, true);
        }

        return virtual_address;
    }

    void register_isr(void (handler)(void*), void *cb){
        add_dynamic_isr(PCI_INT_VECTOR, handler, cb);
    }
}

bool pci_device_t::has_capability(uint8_t capability_id){
    if ((this->header->status & (1 << 4)) == 0) return false; // No capabilities pointer
    pci::PCIHeader0 *hdr0 = (pci::PCIHeader0 *)this->header;
    pci::capability_t *current = (pci::capability_t *)((uint64_t)this->header + hdr0->CapabilitiesPtr);

    while (true) {
        if (current->capability_id == capability_id) return true;
        if (current->next_capability == 0) break;
        current = (pci::capability_t *)((uint64_t)this->header + current->next_capability);
    }

    return false;
}

pci::capability_t *pci_device_t::get_capability(uint8_t capability_id){
    if ((this->header->status & (1 << 4)) == 0) return nullptr; // No capabilities pointer
    pci::PCIHeader0 *hdr0 = (pci::PCIHeader0 *)this->header;
    pci::capability_t *current = (pci::capability_t *)((uint64_t)this->header + hdr0->CapabilitiesPtr);

    while (true) {
        if (current->capability_id == capability_id) return current;
        if (current->next_capability == 0) break;
        current = (pci::capability_t *)((uint64_t)this->header + current->next_capability);
    }

    return nullptr;
}

int pci_device_t::allocate_interrupts(int requested_count) {
    if (this->has_capability(0x11)) {
        // MSI-X
        this->irq_type = pci::INT_MSIX;
        
        // Find the MSI-X MMIO table
        pci::msix_capability_t *capability = (pci::msix_capability_t *)this->get_capability(0x11);
        uint64_t msix_table_ptr = pci::get_device_bar(this->header, capability->table_offset_bir & 0b111) + (capability->table_offset_bir & ~0x7);

        // Allocate the vectors and configure the table
        int count_to_allocate = min(requested_count, 32); 
        
        for (int i = 0; i < count_to_allocate; i++) {
            uint8_t vector = idt_allocate_vector();
            this->idt_vectors[i] = vector;

            uint8_t target_cpu = i % local_cpu_cnt; 

            // Write to the MSI-X hardware table
            volatile uint32_t* entry = (volatile uint32_t*)((uint64_t)msix_table_ptr + (i * 16));
            entry[0] = 0xFEE00000 | (target_cpu << 12); // Address Low
            entry[1] = 0;                               // Address High
            entry[2] = vector;                          // Data (The IDT vector)
            entry[3] = 1;                               // Start with it masked
        }

        capability->message_control |= (1 << 15);

        this->allocated_irq_count = count_to_allocate;
        return count_to_allocate;
    }

    if (this->has_capability(0x05)) {
        // MSI
        this->irq_type = pci::INT_MSI;
        
        pci::msi_capability_t* msi = (pci::msi_capability_t*)this->get_capability(0x05);
        
        uint8_t vector = idt_allocate_vector();
        this->idt_vectors[0] = vector;
        uint8_t target_cpu = 0;

        // Write the Target APIC Address
        msi->message_address_low = 0xFEE00000 | (target_cpu << 12);

        // Safely handle the 32-bit vs 64-bit struct shift
        // Bit 7 of message_control tells us if the device supports 64-bit addresses
        bool is_64bit_capable = (msi->message_control & (1 << 7)) != 0;

        if (is_64bit_capable) {
            msi->message_address_high = 0;
            msi->message_data = vector;
        } else {
            volatile uint16_t* data_ptr = (volatile uint16_t*)&msi->message_address_high;
            *data_ptr = vector;
        }

        // Configure the Control bits
        // Clear bits 4-6 (Multiple Message Enable) because we are only requesting 1 vector
        msi->message_control &= ~(0b111 << 4);

        // Clear bit 0 (MSI Enable) to ensure it starts MASKED (disabled)
        msi->message_control &= ~1;
        
        this->allocated_irq_count = 1; // Fallback to 1
        return 1;
    }

    // LEGACY INTx
    this->irq_type = pci::INT_LEGACY;
    this->idt_vectors[0] = PCI_INT_VECTOR;
    this->allocated_irq_count = 1;
    return 1;
}


void pci_device_t::register_interrupt_handler(int index, void (*handler)(void*), void* ctx, uint8_t target_cpu) {
    if (index >= this->allocated_irq_count) return;
    
    uint8_t actual_idt_vector = this->idt_vectors[index];
    add_dynamic_isr(actual_idt_vector, handler, ctx);

    cpu_local_data *local = bsp_local;

    while (local){
        if (local->cpu_id == target_cpu){
            target_cpu = local->lapic->lapic_id;
            break;
        }

        local = local->next;
    }
    
    if (this->irq_type == pci::INT_MSIX) {
        
        pci::msix_capability_t *capability = (pci::msix_capability_t *)this->get_capability(0x11);
        uint64_t bar_phys = pci::get_device_bar(this->header, capability->table_offset_bir & 0x7);
        uint64_t offset = capability->table_offset_bir & ~0x7UL; 
        uint64_t msix_table_ptr = bar_phys + offset;

        volatile uint32_t* entry = (volatile uint32_t*)(msix_table_ptr + (index * 16));
        
        entry[0] = 0xFEE00000 | (target_cpu << 12); 
        
        entry[1] = 0;
        entry[2] = actual_idt_vector;
        entry[3] = 0;

    } else if (this->irq_type == pci::INT_MSI) {
        
        pci::msi_capability_t* msi_cap = (pci::msi_capability_t*)this->get_capability(0x05);
        if (msi_cap) {
            msi_cap->message_address_low = 0xFEE00000 | (target_cpu << 12);
            msi_cap->message_data = actual_idt_vector;
            msi_cap->message_control |= 1;
        }

    } else if (this->irq_type == pci::INT_LEGACY) {
        this->header->command &= ~(1 << 10);
    }
}
/* ACPI _PRT */

struct irq_info {
    uint32_t gsi;
    bool active_low;
    bool level_triggered;
};

bool routed_gsis[256] = { false }; 

irq_info resolve_link_device(ACPI_HANDLE parent_bus_handle, char* source_name) {
    ACPI_HANDLE link_handle;
    if (ACPI_FAILURE(AcpiGetHandle(parent_bus_handle, source_name, &link_handle))) 
        return {0, false, false};

    ACPI_BUFFER res_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
    ACPI_STATUS status = AcpiGetCurrentResources(link_handle, &res_buffer);
    irq_info info = {0, false, false};

    auto parse_resource = [&](ACPI_BUFFER& buf) -> irq_info {
        uint8_t* ptr = (uint8_t*)buf.Pointer;
        while (ptr) {
            ACPI_RESOURCE* res = (ACPI_RESOURCE*)ptr;
            if (res->Type == ACPI_RESOURCE_TYPE_IRQ) {
                return {
                    res->Data.Irq.Interrupts[0],
                    (res->Data.Irq.Polarity == ACPI_ACTIVE_LOW),
                    (res->Data.Irq.Triggering == ACPI_LEVEL_SENSITIVE)
                };
            }
            if (res->Type == ACPI_RESOURCE_TYPE_EXTENDED_IRQ) {
                return {
                    res->Data.ExtendedIrq.Interrupts[0],
                    (res->Data.ExtendedIrq.Polarity == ACPI_ACTIVE_LOW),
                    (res->Data.ExtendedIrq.Triggering == ACPI_LEVEL_SENSITIVE)
                };
            }
            if (res->Type == ACPI_RESOURCE_TYPE_END_TAG) break;
            ptr += res->Length;
        }
        return {0, false, false};
    };

    if (ACPI_SUCCESS(status)) {
        info = parse_resource(res_buffer);
        AcpiOsFree(res_buffer.Pointer);
    }

    if (info.gsi == 0) {
        res_buffer.Pointer = NULL;
        res_buffer.Length = ACPI_ALLOCATE_BUFFER;
        if (ACPI_SUCCESS(AcpiGetPossibleResources(link_handle, &res_buffer))) {
            info = parse_resource(res_buffer);
            AcpiOsFree(res_buffer.Pointer);
        }
    }

    return info;
}

extern "C" void* isr_stub_table_ptrs[256];

ACPI_STATUS harvest_gsis(ACPI_HANDLE object, UINT32 nesting_level, void *context, void **return_value) {
    ACPI_BUFFER buffer = { ACPI_ALLOCATE_BUFFER, NULL };
    
    if (ACPI_SUCCESS(AcpiGetIrqRoutingTable(object, &buffer))) {
        ACPI_PCI_ROUTING_TABLE* prt = (ACPI_PCI_ROUTING_TABLE*)buffer.Pointer;

        while (prt->Length) {
            irq_info info = {0, false, false};

            if (prt->Source[0] == '\0') {
                info.gsi = prt->SourceIndex;
                info.active_low = true;
                info.level_triggered = true;
            } else {
                // Link Object: Query the object for its actual polarity/trigger
                info = resolve_link_device(object, prt->Source);
            }

            if (info.gsi > 0 && info.gsi < 256 && !routed_gsis[info.gsi]) {
                // Now passing the REAL discovered flags to the APIC
                set_apic_irq(info.gsi, PCI_INT_VECTOR, false, info.level_triggered, info.active_low);
                routed_gsis[info.gsi] = true;
            }

            prt = (ACPI_PCI_ROUTING_TABLE*)((uint8_t*)prt + prt->Length);
        }
        AcpiOsFree(buffer.Pointer);
    }
    return AE_OK;
}

void init_pci_interrupt_routing() {
    _set_bsp_interrupt_service_routine(isr_stub_table_ptrs[PCI_INT_VECTOR], PCI_INT_VECTOR, IDT_TA_InterruptGate, 0x08);
    AcpiWalkNamespace(ACPI_TYPE_DEVICE, 
                      ACPI_ROOT_OBJECT, 
                      UINT32_MAX, 
                      harvest_gsis, 
                      NULL, NULL, NULL);
}