#include "kernelUtil.h"
#include "gdt/gdt.h"
#include "interrupts/IDT.h"
#include "interrupts/interrupts.h"
#include "IO.h"


KernelInfo kernelInfo; 


void PrepareACPI(BootInfo* bootInfo){
    ACPI::SDTHeader* xsdt = (ACPI::SDTHeader*)(bootInfo->rsdp->XSDTAddress);
    ACPI::MCFGHeader* mcfg = (ACPI::MCFGHeader*)ACPI::FindTable(xsdt, (char*)"MCFG");

    PCI::EnumeratePCI(mcfg);
}

void PrepareMemory(BootInfo* bootInfo){
    uint64_t mMapEntries = bootInfo->mMapSize / bootInfo->mMapDescSize;

    GlobalAllocator = PageFrameAllocator();
    GlobalAllocator.ReadEFIMemoryMap(bootInfo->mMap, bootInfo->mMapSize, bootInfo->mMapDescSize);

    uint64_t kernelSize = (uint64_t)&_KernelEnd - (uint64_t)&_KernelStart;
    uint64_t kernelPages = (uint64_t)kernelSize / 4096 + 1;

    GlobalAllocator.LockPages(&_KernelStart, kernelPages);

    PageTable* PML4 = (PageTable*)GlobalAllocator.RequestPage();
    if (PML4 == NULL){
        globalRenderer->Clear(0x004f57);

        globalRenderer->cursorPos.X = 0;
        globalRenderer->cursorPos.Y = 0;

        globalRenderer->print("Kernel Panic\n\n");

        globalRenderer->print("MEMORY MAPPING FAILED, PML4 TABLE IS NULL!");
        globalRenderer->print("\nTotal RAM: ");
        globalRenderer->print(toString((GetMemorySize(bootInfo->mMap, mMapEntries, bootInfo->mMapDescSize)/1024)/1024));
        globalRenderer->print("Free RAM: ");
        globalRenderer->print(toString((GlobalAllocator.GetFreeRAM()/1024)/1024));
        globalRenderer->print(" MB\nReserved RAM: ");
        globalRenderer->print(toString(((GlobalAllocator.GetReservedRAM()/1024)/1024)));
        globalRenderer->print(" MB\nUsed RAM: ");
        globalRenderer->print(toString(((GlobalAllocator.GetUsedRAM()/1024)/1024)));
        globalRenderer->print(" MB\n");
        while(1);
    }
    memset(PML4, 0, 0x1000);

    globalPTM = PageTableManager(PML4);

    for (uint64_t t = 0; t < GetMemorySize(bootInfo->mMap, mMapEntries, bootInfo->mMapDescSize); t+= 0x1000){
        globalPTM.MapMemory((void*)t, (void*)t);
    }

    uint64_t fbBase = (uint64_t)bootInfo->framebuffer->BaseAddress;
    uint64_t fbSize = (uint64_t)bootInfo->framebuffer->BufferSize + 0x1000;
    GlobalAllocator.LockPages((void*)fbBase, fbSize/ 0x1000 + 1);
    for (uint64_t t = fbBase; t < fbBase + fbSize; t += 4096){
        globalPTM.MapMemory((void*)t, (void*)t);
    }

    asm ("mov %0, %%cr3" : : "r" (PML4));

    kernelInfo.pageTableManager = &globalPTM;
}

IDTR idtr;

void SetIDTGate(void* handler, uint8_t entryOffset, uint8_t type_attr, uint8_t selector){
    IDTDescEntry* interrupt = (IDTDescEntry*)(idtr.Offset + entryOffset * sizeof(IDTDescEntry));
    interrupt->SetOffset((uint64_t)handler);
    interrupt->type_attr = type_attr;
    interrupt->selector = selector;
}
void PrepareInterrupts(){
    idtr.Limit = 0x0FFF;
    idtr.Offset = (uint64_t)GlobalAllocator.RequestPage();

    SetIDTGate((void*)PageFault_Handler, 0xE, IDT_TA_InterruptGate, 0x08); //Page Fault
    SetIDTGate((void*)DoubleFault_Handler, 0x8, IDT_TA_InterruptGate, 0x08); //Double Fault
    SetIDTGate((void*)GPFault_Handler, 0xd, IDT_TA_InterruptGate, 0x08); //General Protection Fault
    SetIDTGate((void*)KeyboardInt_Handler, 0x21, IDT_TA_InterruptGate, 0x08); //PS2 Keyboard
    SetIDTGate((void*)MouseInt_Handler, 0x2c, IDT_TA_InterruptGate, 0x08); //PS2 Mouse Handler
    SetIDTGate((void*)PitInt_Handler, 0x20, IDT_TA_InterruptGate, 0x08); //PIT CHIP Handler
    SetIDTGate((void*)APICInt_handler, 0x30, IDT_TA_InterruptGate, 0x08); //APIC Handler
    SetIDTGate((void*)APICInit_Handler, 0x32, IDT_TA_InterruptGate, 0x08); //APIC INIT

    asm ("lidt %0" : : "m" (idtr));

    RemapPIC();
}
BasicRenderer r = BasicRenderer(NULL, NULL);
KernelInfo InitializeKernel(BootInfo* bootInfo){
    r = BasicRenderer(bootInfo->framebuffer, bootInfo->psf1_Font);
    globalRenderer = &r;
    GDTDescriptor gdtDescriptor;
    gdtDescriptor.Size = sizeof(GDT) - 1;
    gdtDescriptor.Offset = (uint64_t)&DefaultGDT;
    LoadGDT(&gdtDescriptor);
    PrepareMemory(bootInfo);
    memset(bootInfo->framebuffer->BaseAddress, 0, bootInfo->framebuffer->BufferSize);
    PrepareInterrupts();
    InitializeHeap((void*)0x0000100000000000, 0x10);
    globalRenderer->Clear(0x0f0024);
    InitPS2Mouse();
    PrepareACPI(bootInfo);

    outb(PIC1_DATA, 0b11111000);
    outb(PIC2_DATA, 0b11101111);

    InitLocalAPIC();


    asm ("sti");

    return kernelInfo;
}