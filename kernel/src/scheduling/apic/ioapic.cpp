#include <scheduling/apic/ioapic.h>
#include <paging/PageTableManager.h>
#include <acpi.h>
#include <memory.h>

int io_apic_count = 0;
io_apic_t* io_apic_list = nullptr;
io_apic_iso_t* io_apic_iso_list = nullptr;
madt_header_t* madt_header = nullptr;

void add_to_io_apic_list(io_apic_t* ioapic){
    ioapic->next = nullptr;

    io_apic_t* c = io_apic_list;

    if (!c) {
        io_apic_list = ioapic;
        return;
    }

    while (c->next) c = c->next;
    c->next = ioapic;

    io_apic_count++;
}


void add_to_io_apic_iso_list(io_apic_iso_t* iso){
    iso->next = nullptr;

    io_apic_iso_t* c = io_apic_iso_list;

    if (!c) {
        io_apic_iso_list = iso;
        return;
    }

    while (c->next) c = c->next;
    c->next = iso;
}

void init_madt(){
    madt_header = (madt_header_t*)ACPI::FindTable(xsdt, (char*)"APIC");
}

void setup_io_apic(madt_io_apic_entry* entry){
    uint64_t physical_address = entry->APICAddress;
    uint64_t virtual_address = physical_to_virtual(physical_address); // It always resides in physical memory.

    globalPTM.MapMemory((void*)virtual_address, (void*)physical_address);
    
    io_apic_t* io_apic = new io_apic_t;
    io_apic->id = io_apic_count;

    io_apic->base_address = virtual_address;
    
    uint32_t version_register = io_apic->read_register(IOAPIC_REG_VER);
    uint8_t version = version_register & 0xFF;
    io_apic->base_gsi = entry->GSIBase;
    io_apic->max_gsi = ((version_register >> 16) & 0xFF) + io_apic->base_gsi;
    io_apic->next = nullptr;

    add_to_io_apic_list(io_apic);
}

void register_io_apic_iso(madt_io_apic_iso_entry* entry){
    io_apic_iso_t* iso = new io_apic_iso_t;
    iso->bus_source = entry->BusSource;
    iso->irq_source = entry->IRQSource;
    iso->gsi = entry->GSI;
    iso->flags = entry->Flags;
    iso->next = nullptr;

    add_to_io_apic_iso_list(iso);
}

#include <kstdio.h>
void init_io_apic(){
    init_madt();

    // Loop through the MADT
    uint16_t offset = sizeof(madt_header_t);
    int32_t remaining = madt_header->Length - offset;

    while(remaining > 0){
        madt_entry_t* entry = (madt_entry_t*)((uint64_t)madt_header + offset);
        if (entry->Type == 1){
            setup_io_apic((madt_io_apic_entry*)entry);
        }else if (entry->Type == 2){
            madt_io_apic_iso_entry* iso = (madt_io_apic_iso_entry*)entry;
            //kprintf("ISO %d %d %d -> %d\n", iso->BusSource, iso->IRQSource, iso->Flags, iso->GSI);
            register_io_apic_iso((madt_io_apic_iso_entry*)entry);
        }

        offset += entry->Length;
        remaining -= entry->Length;
    }
}

#include <kstdio.h>
uint32_t _apic_irq_redirect(uint8_t irq, bool *level_sense, bool *active_low){
    //Loop through ISO
    int i = 0;
    for (io_apic_iso_t* iso = io_apic_iso_list; iso != nullptr; iso = iso->next){
        if (iso->irq_source == irq) {
            if (!iso->bus_source){
                if (iso->flags & 2){
                    *active_low = true;
                } else {
                    *active_low = false;
                }

                if (iso->flags & 8){
                    *level_sense = true;
                } else {
                    *level_sense = false;
                }
            }
            return iso->gsi;
        }
    }
    
    return irq;
}

void set_apic_irq(uint8_t irq, uint8_t vector, bool mask, bool l_sense, bool act_low, bool no_redirection){
    bool level_sense = l_sense;
    bool active_low = act_low;

    uint32_t gsi = no_redirection ? irq : _apic_irq_redirect(irq, &level_sense, &active_low);

    uint32_t low = 0;
    low |= vector; // Vector
    low |= 0 << 8; // Delivery Mode
    low |= 0 << 11; // Destination Mode
    low |= 0 << 12; // Delivery Status
    low |= 0 << 13; // Pin Polarity
    low |= 0 << 14; // IRR
    low |= 0 << 15; // Trigger Mode
    if (mask) low |= 1U << 16; // Mask

    if (level_sense) low |= 1 << 15; // Trigger Mode: 1 = Level Sensitive
    if (active_low) low |= 1 << 13; // Pin Polarity: 1 = Active Low
    
    uint32_t high = 0;
    high |= 0 << 24; // Destination

    for (io_apic_t* apic = io_apic_list; apic != nullptr; apic = apic->next){
        if (apic->base_gsi <= gsi && apic->max_gsi >= gsi){
            uint32_t relative_offset = (gsi - apic->base_gsi) * 2;
            apic->write_register(IOAPIC_REDIRECTION_BASE + relative_offset, low);
            apic->write_register(IOAPIC_REDIRECTION_BASE + relative_offset + 1, high);
            break;
        }
    }
}

uint32_t io_apic_t::read_register(uint16_t reg){
    *(uint32_t*)(base_address + IOREGSEL) = reg;
    return *(uint32_t*)(base_address + IOWIN);
}

void io_apic_t::write_register(uint16_t reg, uint32_t value){
    *(uint32_t*)(base_address + IOREGSEL) = reg;
    *(uint32_t*)(base_address + IOWIN) = value;
}