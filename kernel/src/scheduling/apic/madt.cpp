#include "madt.h"
MADT* madt;
void printn(const char* txt, uint8_t num){
    kprintf(txt);
    kprintf(": ");
    kprintf(toString(num));
    kprintf("\n");
}
void printh(const char* txt, uint8_t num){
    kprintf(txt);
    kprintf(": 0x");
    kprintf(toHString(num));
    kprintf("\n");
}
void printh(const char* txt, uint16_t num){
    kprintf(txt);
    kprintf(": 0x");
    kprintf(toHString(num));
    kprintf("\n");
}
void printh(const char* txt, uint32_t num){
    kprintf(txt);
    kprintf(": 0x");
    kprintf(toHString(num));
    kprintf("\n");
}
void printh(const char* txt, uint64_t num){
    kprintf(txt);
    kprintf(": 0x");
    kprintf(toHString(num));
    kprintf("\n");
}
void MADT::EnumerateEntry(void* entryAddress){
    Entry* entry = (Entry*)entryAddress;
    switch (entry->Type){
        case 0:{ //LAPIC
            LAPIC* lapic = (LAPIC*)((uint64_t)entryAddress + sizeof(Entry));
            printn("LAPIC | Processor ID", lapic->ProcessorID);
            printn("LAPIC | APIC ID", lapic->APICID);
            printh("LAPIC | Flags", (uint8_t)lapic->Flags);
            break;
        }
        case 1:{// I/O APIC
            IOAPIC* ioApic = (IOAPIC*)((uint64_t)entryAddress + sizeof(Entry));
            printn("IO APIC | APIC ID", ioApic->APICID);
            printh("IO APIC | APIC Address", ioApic->APICAddress);
            printh("IO APIC | GSI Base Address", ioApic->GSIBase);
            break;
        }
        case 2:{ // I/O APIC Interrupt Source Override
            APICISO* ioApicISO = (APICISO*)((uint64_t)entryAddress + sizeof(Entry));
            printh("I/O APIC ISO | Bus Source", ioApicISO->BusSource);
            printh("I/O APIC ISO | IRQ Source", ioApicISO->IRQSource);
            printh("I/O APIC ISO | GSI Base Address", ioApicISO->GSI);
            printh("I/O APIC ISO | Flags", ioApicISO->Flags);
            break;
        }
        case 3:{ //I/O APIC Non-maskable interrupt source
            IOAPICNMIS* ioApicNMIS = (IOAPICNMIS*)((uint64_t)entryAddress + sizeof(Entry));
            printh("I/O APIC NMIS | Non-Maskable interrupt Source", ioApicNMIS->NMISource);
            printh("I/O APIC NMIS | Flags", ioApicNMIS->Flags);
            printh("I/O APIC NMIS | GSI Base Address", ioApicNMIS->GSI);
            break;
        }
        case 4:{//Local APIC Non-maskable interrupts
            LAPICNMI* lApicNMI = (LAPICNMI*)((uint64_t)entryAddress + sizeof(Entry));
            printh("Local APIC Non-maskable interrupts | Processor ID", lApicNMI->ProcessorID);
            printh("Local APIC Non-maskable interrupts | Flags", lApicNMI->Flags);
            printh("Local APIC Non-maskable interrupts | LINT", lApicNMI->LINT);
            break;
        }
        case 5:{ //Local APIC Address Override
            LAPPICAddrOverride* lApicAO = (LAPPICAddrOverride*)((uint64_t)entryAddress + sizeof(Entry));
            printh("Local APIC Address Override | LAPIC Address (64 Bit)", lApicAO->LAPICAddress);
            break;
        }
        case 9:{ //Processor Local x2APIC
            PLX2APIC* x2Apic = (PLX2APIC*)((uint64_t)entryAddress + sizeof(Entry));
            printn("X2 APIC | Processor's local x2APIC ID ", x2Apic->LX2Address);
            printn("X2 APIC | ACPI ID", x2Apic->ACPIID);
            printh("X2 APIC | Flags", x2Apic->Flags);
            break;
        }
    }
}

const char* nS(char* str, uint length){
    char* str2 = (char*)malloc(length);
    for (uint i = 0; i < length; i++){
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
    madt = this;
    MADTBuffer = ACPI::FindTable(xsdt, (char*)"APIC");
    MADTHdr = (MADTHeader*)MADTBuffer;
    
    if (MADTHdr->Length == 0) {
        kprintf("MADT Length is 0, check the MADT data.\n");
        return;
    }

    //EnumerateEntries();
}