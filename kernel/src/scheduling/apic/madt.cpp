#include <scheduling/apic/madt.h>


MADT* madt = nullptr;

void MADT::EnumerateEntry(void* entryAddress){
    Entry* entry = (Entry*)entryAddress;
    switch (entry->Type){
        case 0:{ //LAPIC
            kprintf(0xFF0000, "-------------- LOCAL APIC --------------\n");
            LAPIC* lapic = (LAPIC*)((uint64_t)entryAddress + sizeof(Entry));
            kprintf(0x00FF00, "LAPIC | Processor ID: %d\n", lapic->ProcessorID);
            kprintf(0x00FF00, "LAPIC | APIC ID: %d\n", lapic->APICID);
            kprintf(0x00FF00, "LAPIC | Flags: %hh\n", (uint8_t)lapic->Flags);
            break;
        }
        case 1:{// I/O APIC
            kprintf(0xFF0000, "--------------- I/O APIC ---------------\n");

            IOAPIC* ioApic = (IOAPIC*)((uint64_t)entryAddress + sizeof(Entry));
            kprintf(0x00FF00, "IO APIC | APIC ID: %d\n", ioApic->APICID);
            kprintf(0x00FF00, "IO APIC | APIC Address: %hh\n", ioApic->APICAddress);
            kprintf(0x00FF00, "IO APIC | GSI Base Address: %hh\n", ioApic->GSIBase);
            break;
        }
        case 2:{ // I/O APIC Interrupt Source Override
            break;
            kprintf(0xFF0000, "--- I/O APIC Interrupt Source Override ---\n");

            APICISO* ioApicISO = (APICISO*)((uint64_t)entryAddress + sizeof(Entry));
            kprintf(0x00FF00, "I/O APIC ISO | Bus Source: %hh\n", ioApicISO->BusSource);
            kprintf(0x00FF00, "I/O APIC ISO | IRQ Source: %hh\n", ioApicISO->IRQSource);
            kprintf(0x00FF00, "I/O APIC ISO | GSI Base Address: %hh\n", ioApicISO->GSI);
            kprintf(0x00FF00, "I/O APIC ISO | Flags: %hh\n", ioApicISO->Flags);
            break;
        }
        case 3:{ //I/O APIC Non-maskable interrupt source
            break;
            kprintf(0xFF0000, "----------- I/O APIC NMI SOURCE -----------\n");

            IOAPICNMIS* ioApicNMIS = (IOAPICNMIS*)((uint64_t)entryAddress + sizeof(Entry));
            kprintf(0x00FF00, "I/O APIC NMIS | Non-Maskable interrupt Source: %hh\n", ioApicNMIS->NMISource);
            kprintf(0x00FF00, "I/O APIC NMIS | Flags: %hh\n", ioApicNMIS->Flags);
            kprintf(0x00FF00, "I/O APIC NMIS | GSI Base Address: %hh\n", ioApicNMIS->GSI);
            break;
        }
        case 4:{//Local APIC Non-maskable interrupts
            break;
            kprintf(0xFF0000, "------------- Local APCI NMIs -------------\n");

            LAPICNMI* lApicNMI = (LAPICNMI*)((uint64_t)entryAddress + sizeof(Entry));
            kprintf(0x00FF00, "Local APIC Non-maskable interrupts | Processor ID: %d\n", lApicNMI->ProcessorID);
            kprintf(0x00FF00, "Local APIC Non-maskable interrupts | Flags: %hh\n", lApicNMI->Flags);
            kprintf(0x00FF00, "Local APIC Non-maskable interrupts | LINT: %hh\n", lApicNMI->LINT);
            break;
        }
        case 5:{ //Local APIC Address Override
            kprintf(0xFF0000, "------ LOCAL APIC Address override --------\n");

            LAPPICAddrOverride* lApicAO = (LAPPICAddrOverride*)((uint64_t)entryAddress + sizeof(Entry));
            kprintf(0x00FF00, "Local APIC Address Override | LAPIC Address (64 Bit): %llx\n", lApicAO->LAPICAddress);
            break;
        }
        case 9:{ //Processor Local x2APIC
            kprintf(0xFF0000, "-------------- LOCAL x2APIC --------------\n");

            PLX2APIC* x2Apic = (PLX2APIC*)((uint64_t)entryAddress + sizeof(Entry));
            kprintf(0x00FF00, "X2 APIC | Processor's local x2APIC ID: %llx\n", x2Apic->LX2Address);
            kprintf(0x00FF00, "X2 APIC | ACPI ID: %d\n", x2Apic->ACPIID);
            kprintf(0x00FF00, "X2 APIC | Flags: %hh\n", x2Apic->Flags);
            break;
        }
    }
}

const char* nS(char* str, uint32_t length){
    char* str2 = (char*)malloc(length);
    for (uint32_t i = 0; i < length; i++){
        if (i == length){
            str2[i] = '\0';
            break;
        }
        str2[i] = str[i];
    }
    return str2;
}
void MADT::EnumerateEntries(){
    uint16_t offset = 0x2c;
    int32_t remaining = MADTHdr->Length - offset;
    while(remaining > 0){
        Entry* entry = (Entry*)((uint64_t)MADTBuffer + offset);
        EnumerateEntry((uint64_t*)entry);
        offset += entry->Length;
        remaining -= entry->Length;
    }
}

Entry* MADT::FindLApic(uint8_t ApicID){
    uint16_t offset = 0x2c;
    int32_t remaining = MADTHdr->Length - offset;
    Entry* apic = nullptr;
    while(remaining > 0){
        Entry* entry = (Entry*)((uint64_t)MADTBuffer + offset);
        if (entry->Type == 0){
            LAPIC* lapic = (LAPIC*)(*(uint64_t*)entry + sizeof(Entry));
            if (lapic->APICID == ApicID){
                apic = entry;
                break;
            }
        }
        offset += entry->Length;
        remaining -= entry->Length;
    }
    return apic;
}
ISOEntryTable MADT::FindAPICISO(){
    uint16_t offset = 0x2c;
    int32_t remaining = MADTHdr->Length - offset;
    Entry** entries = (Entry**)GlobalAllocator.RequestPages((uint16_t)((25 * sizeof(Entry*) / 0x1000) + 1));
    uint16_t length = 0;
    while(remaining > 0){
        Entry* entry = (Entry*)((uint64_t)MADTBuffer + offset);
        if (entry->Type == 2){
            entries[length] = entry;
            length++;
        }
        offset += entry->Length;
        remaining -= entry->Length;
    }
    ISOEntryTable result = {entries, length};
    return result;
}
Entry* MADT::FindIOApic(){
    uint16_t offset = 0x2c;
    int32_t remaining = MADTHdr->Length - offset;
    Entry* apic = nullptr;
    while(remaining > 0){
        Entry* entry = (Entry*)((uint64_t)MADTBuffer + offset);
        if (entry->Type == 1){
            apic = entry;
            break;
        }
        offset += entry->Length;
        remaining -= entry->Length;
    }
    return apic;
}

MADT::MADT(){
    MADTBuffer = ACPI::FindTable(xsdt, (char*)"APIC");
    MADTHdr = (MADTHeader*)MADTBuffer;
    
    if (MADTHdr->Length == 0) {
        kprintf(0x00FF00, "MADT Length is 0, check the MADT data.\n");
        return;
    }
}