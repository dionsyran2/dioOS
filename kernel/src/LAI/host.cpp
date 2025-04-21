#include <lai/host.h>
#include "../panic.h"
#include "../BasicRenderer.h"
#include "../paging/PageTableManager.h"
#include "../acpi.h"
#include "../cstr.h"
#include "../IO.h"
#include "../pci.h"
#include "../scheduling/apic/apic.h"

void laihost_log(int level, const char *msg){
    return;
    switch (level)
    {
        case LAI_DEBUG_LOG:
            globalRenderer->printf(0xFFD700, "[LAI] ");
            globalRenderer->printf(msg);
            globalRenderer->printf("\n");
            break;
        case LAI_WARN_LOG:
            globalRenderer->printf(0x00FF00, "[LAI] ");
            globalRenderer->printf(msg);
            globalRenderer->printf("\n");
            break;
        default:
            globalRenderer->printf("[LAI]");
            globalRenderer->printf(msg);
            globalRenderer->printf("\n");
            break;
    }
}

__attribute__((noreturn)) void laihost_panic(const char *msg){
    panic("LAI", msg);
}

/* Self-explanatory */
void *laihost_malloc(size_t size){
    if (size == 0) size = 1;
    return malloc(size);
}
void *laihost_realloc(void *oldptr, size_t newsize, size_t oldsize){
    if (newsize == 0) newsize = 1;
    if (oldptr != nullptr) free(oldptr);
    return malloc(newsize);
}
void laihost_free(void *ptr, size_t size){
    if (size == 0) size = 1;
    if (ptr != nullptr) free(ptr);
}

/* Maps count bytes from the given physical address and returns
   a virtual address that can be used to access the memory. */
void *laihost_map(size_t address, size_t count){
    for (size_t i = 0; i < count; i++){
        globalPTM.MapMemory((void*)(address + i), (void*)(address + i));
    }
    return (void*)address;
}

/* Unmaps count bytes from the given virtual address.
   LAI only calls this on memory that was previously mapped by laihost_map(). */
void laihost_unmap(void *pointer, size_t count){
    return;
}

/* Returns the (virtual) address of the n-th table that has the given signature,
   or NULL when no such table was found. */
void* find_table(const char *sig, size_t index){
    int entries = (xsdt->Length - sizeof(ACPI::SDTHeader)) / 8;
    int y = 0;
    char* sign = (char*)sig;
    for(int t = 0; t < entries; t++){
        uint64_t entryAddress = *(uint64_t*)((uint64_t)xsdt + sizeof(ACPI::SDTHeader) + (t * sizeof(uint64_t)));
        ACPI::SDTHeader* newSDTHeader = (ACPI::SDTHeader*)entryAddress;
        for (int i=0;i<4;i++){
            if (newSDTHeader->Signature[i] != sign[i]){
                break;
            }
            if (i == 3) {
                if (index != y){
                    y++;
                    break;
                }
                globalPTM.MapMemory((void*)entryAddress, (void*)entryAddress);
                return (void*)entryAddress;
            };
        }
    }
    return nullptr;
}

void* locate_dsdt(){
    void* fadt = find_table("FACP", 0);
    ACPI::FADT_HDR* fadt_hdr = (ACPI::FADT_HDR*)fadt;

    ACPI::SDTHeader* dsdt_hdr = (ACPI::SDTHeader*)fadt_hdr->X_Dsdt;
    if (strncmp((const char*)dsdt_hdr->Signature, "DSDT", 3)){
        dsdt_hdr = (ACPI::SDTHeader*)fadt_hdr->Dsdt;
        if (strncmp((const char*)dsdt_hdr->Signature, "DSDT", 3)){
            return nullptr;
        }
    }
    return (void*) dsdt_hdr;
}

void* laihost_scan(const char *sig, size_t index){
    if (!strncmp(sig, "DSDT", 3)){
        return locate_dsdt();
    }else{
        return find_table(sig, index);
    }
}


//Remove these to test only initialization!!!
/* Write a byte/word/dword to the given I/O port. */
void laihost_outb(uint16_t port, uint8_t val){
    outb(port, val);
}
void laihost_outw(uint16_t port, uint16_t val){
    outw(port, val);
}
void laihost_outd(uint16_t port, uint32_t val){
    outd(port, val);
}

/* Read a byte/word/dword from the given I/O port. */
uint8_t laihost_inb(uint16_t port){
    return inb(port);
}
uint16_t laihost_inw(uint16_t port){
    return inw(port);
}
uint32_t laihost_ind(uint16_t port){
    return ind(port);
}


/* Write a byte/word/dword to the given device's PCI configuration space
   at the given offset. */


void* GetSegment(int seg){
    ACPI::MCFGHeader* mcfg = (ACPI::MCFGHeader*)find_table("MCFG", 0);
    int entries = ((mcfg->Header.Length) - sizeof(ACPI::MCFGHeader)) / sizeof(ACPI::DeviceConfig);
    if (entries < seg) return nullptr;

    ACPI::DeviceConfig* newDeviceConfig = (ACPI::DeviceConfig*)((uint64_t)mcfg + sizeof(ACPI::MCFGHeader) + (sizeof(ACPI::DeviceConfig) * seg));
    return (void*)newDeviceConfig->BaseAddress;
}

void* GetBus(void* seg, int bus){
    uint64_t offset = bus << 20;

    uint64_t busAddress = (uint64_t)seg + offset;

    PCI::PCIDeviceHeader* pciDeviceHeader = (PCI::PCIDeviceHeader*)busAddress;

    if (pciDeviceHeader->DeviceID == 0) return nullptr;
    if (pciDeviceHeader->DeviceID == 0xFFFF) return nullptr;

    return (void*)busAddress;
}

void* GetSlot(void* base, int slot){
    uint64_t offset = slot << 15;

    uint64_t deviceAddress = (uint64_t)base + offset;

    PCI::PCIDeviceHeader* pciDeviceHeader = (PCI::PCIDeviceHeader*)deviceAddress;

    if (pciDeviceHeader->DeviceID == 0) return nullptr;
    if (pciDeviceHeader->DeviceID == 0xFFFF) return nullptr;

    return (void*)deviceAddress;
}

void* GetFunction(void* base, int function){
    uint64_t offset = function << 12;

    uint64_t functionAddress = (uint64_t)base + offset;

    PCI::PCIDeviceHeader* pciDeviceHeader = (PCI::PCIDeviceHeader*)functionAddress;

    if (pciDeviceHeader->DeviceID == 0) return nullptr;
    if (pciDeviceHeader->DeviceID == 0xFFFF) return nullptr;

    return (void*)functionAddress;
}

void* GetPCIBaseAddress(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun){
    void* segment = GetSegment(seg);
    if (segment == nullptr) return nullptr;

    void* busAddr = GetBus(segment, bus);
    if (busAddr == nullptr) return nullptr;

    void* slotAddr = GetSlot(busAddr, slot);
    if (slotAddr == nullptr) return nullptr;

    return GetFunction(slotAddr, fun);
}


void laihost_pci_writeb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint8_t val){
    void* addr = (void*)((uint64_t)GetPCIBaseAddress(seg, bus, slot, fun) + offset);
    *(uint8_t*)addr = val;
}
void laihost_pci_writew(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint16_t val){
    void* addr = (void*)((uint64_t)GetPCIBaseAddress(seg, bus, slot, fun) + offset);
    *(uint16_t*)addr = val;
}
void laihost_pci_writed(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint32_t val){
    void* addr = (void*)((uint64_t)GetPCIBaseAddress(seg, bus, slot, fun) + offset);
    *(uint32_t*)addr = val;
}

/* Read a byte/word/dword from the given device's PCI configuration space
   at the given offset. */
uint8_t laihost_pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset){
    void* addr = (void*)((uint64_t)GetPCIBaseAddress(seg, bus, slot, fun) + offset);
    return *(uint8_t*)addr;
}
uint16_t laihost_pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset){
    void* addr = (void*)((uint64_t)GetPCIBaseAddress(seg, bus, slot, fun) + offset);
    return *(uint16_t*)addr;
}
uint32_t laihost_pci_readd(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset){
    void* addr = (void*)((uint64_t)GetPCIBaseAddress(seg, bus, slot, fun) + offset);
    return *(uint32_t*)addr;
}

/* Sleeps for the given amount of milliseconds. */
void laihost_sleep(uint64_t ms){
    Sleep(ms);
}

/* Returns a monotonic count in 100ns increments */
uint64_t laihost_timer(void){
    return APICticsSinceBoot * 1000000;
}