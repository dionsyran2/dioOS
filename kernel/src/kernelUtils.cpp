#include "kernel.h"

ACPI::RSDP2* rsdp;
struct multiboot_tag_framebuffer* fb = nullptr;
struct multiboot_tag_new_acpi* acpi;
void PrepareACPI(multiboot_tag_new_acpi* acpi){
    rsdp = (ACPI::RSDP2*)acpi->rsdp;
    globalPTM.MapMemory((void*)rsdp, (void*)rsdp);
    ACPI::MCFGHeader* mcfg = (ACPI::MCFGHeader*)ACPI::FindTable(xsdt, (char*)"MCFG");
    globalPTM.MapMemory((void*)mcfg, (void*)mcfg);
    PCI::EnumeratePCI(mcfg);
}

void InitializeMemory(multiboot_tag_efi_mmap* mmap){
    GlobalAllocator = PageFrameAllocator();
    GlobalAllocator.ReadEFIMemoryMap((EFI_MEMORY_DESCRIPTOR*)mmap->efi_mmap, mmap->size, mmap->descr_size);

    uint64_t kernelSize = (uint64_t)&_KernelEnd - (uint64_t)&_KernelStart;
    uint64_t kernelPages = (uint64_t)kernelSize / 4096 + 1;

    GlobalAllocator.LockPages(&_KernelStart, kernelPages);
    //GlobalAllocator.LockPages((void*)&ap_start, ((((uint64_t)&ap_start_end) - ((uint64_t)&ap_start)) / 0x1000) + 1); //lock the ap trampoline code

    PageTable* PML4 = (PageTable*)GlobalAllocator.RequestPage();
    memset(PML4, 0, 0x1000);

    globalPTM = PageTableManager(PML4);

    /*size_t entryCount = (mmap->size - sizeof(multiboot_tag_efi_mmap)) / mmap->descr_size;
    for (uint64_t t = 0; t < GetMemorySize((EFI_MEMORY_DESCRIPTOR*)mmap->efi_mmap, entryCount, mmap->descr_size); t+= 0x1000){
        globalPTM.MapMemory((void*)t, (void*)t); // Map the entire memory 1:1
    }*/

    size_t mMapEntries = (mmap->size - sizeof(multiboot_tag_efi_mmap)) / mmap->descr_size;
    //Map all of the conventional memory
    for (int i = 0; i < mMapEntries; i++){
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)mmap->efi_mmap + (i * mmap->descr_size));
        if (1/*desc->type == 7 || desc->type == 2 || desc->type == 4 || desc->type == 5 || desc->type == 6 || desc->type == 3 || desc->type == 9*/){
            for (int o = 0; o < desc->numPages; o++){
                globalPTM.MapMemory((void*)((uint64_t)desc->physAddr + (o * 0x1000)), (void*)((uint64_t)desc->physAddr + (o * 0x1000)));
            }
        }
    }

    
    uint64_t fbBase = (uint64_t)fb->common.framebuffer_addr;
    uint64_t fbSize = (fb->common.framebuffer_width * fb->common.framebuffer_height * fb->common.framebuffer_bpp) + 0x1000;
    GlobalAllocator.LockPages((void*)fbBase, fbSize / 0x1000 + 1);
    for (uint64_t t = fbBase; t < fbBase + fbSize; t += 4096){
        globalPTM.MapMemory((void*)t, (void*)t);
    }

   asm volatile ("mov %0, %%cr3" : : "r" (PML4));
}
bool set = false;
uint64_t pagesz = 0;
PSF1_FONT* font;
void Initialize(multiboot_info* bootInfo){
    struct PSF1_FONT psf1_font;
    size_t add_size = 0;
    struct multiboot_tag_module* module;
    // NOTE: We set i to 8 to skip size and reserved fields:
    for (size_t i = 8; i < bootInfo->size; i += add_size) {
        struct multiboot_tag* tag = (struct multiboot_tag *)((uint8_t *)bootInfo + i);
        
        switch (tag->type)
        {
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
                fb = (struct multiboot_tag_framebuffer*)tag;
                break;
            }
            case MULTIBOOT_TAG_TYPE_EFI_MMAP: {
                struct multiboot_tag_efi_mmap* mmap = (struct multiboot_tag_efi_mmap*)tag;
                InitializeMemory(mmap);
                break;
            }
            case MULTIBOOT_TAG_TYPE_MODULE:{
                module = (struct multiboot_tag_module*)tag;
                struct PSF1_HEADER* psf1_hdr = (struct PSF1_HEADER*)module->mod_start;
                if (psf1_hdr->magic[0] != PSF1_MAGIC0 || psf1_hdr->magic[1] != PSF1_MAGIC1) break;
                psf1_font.psf1_Header = psf1_hdr;
                psf1_font.glyphBuffer = (void*)(module->mod_start + sizeof(PSF1_HEADER));
                break;
            }
            case MULTIBOOT_TAG_TYPE_ACPI_NEW:{
                acpi = (struct multiboot_tag_new_acpi*)tag;
                break;
            }
            default: {
                serialPrint(COM1, toString(tag->type));
                serialPrint(COM1, "-");
                break;
            }
        }

        add_size = tag->size;

        // Align the size to 8 bytes.
        if ((add_size % 8) != 0)
			add_size += (8 - add_size % 8);
    }

    GlobalAllocator.LockPages((void*)module->mod_start, (module->mod_end - module->mod_start) / 0x1000); 
    font = (PSF1_FONT*)malloc(sizeof(PSF1_FONT));
    font->psf1_Header = psf1_font.psf1_Header;
    font->glyphBuffer = psf1_font.glyphBuffer;
}

IDTR idtr;

extern "C" void SetIDTGate(void* handler, uint8_t entryOffset, uint8_t type_attr, uint8_t selector){
    IDTDescEntry* interrupt = (IDTDescEntry*)(idtr.Offset + entryOffset * sizeof(IDTDescEntry));
    interrupt->SetOffset((uint64_t)handler);
    interrupt->type_attr = type_attr;
    interrupt->selector = selector;
    interrupt->ist = 0;
}

void PrepareInterrupts(){
    idtr.Limit = 0x0FFF;
    idtr.Offset = (uint64_t)GlobalAllocator.RequestPage();
    memset((void*)idtr.Offset, 0, 0x1000);
    SetIDTGate((void*)DivisionError_Handler, 0x0, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)BoundRangeExceed_Handler, 0x5, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)InvalidOpcode_Handler, 0x6, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)DeviceNotAvailable_Handler, 0x7, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)InvalidTSS_Handler, 0xA, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)SegmentNotPresent_Handler, 0xB, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)StackSegFault_Handler, 0xC, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)x87FloatingPointEx_Handler, 0x10, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)AlignementCheck_Handler, 0x11, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)SIMDFloatPointEx_Handler, 0x13, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)VirtualizationEx_Handler, 0x14, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)CPEx_Handler, 0x15, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)HypervisorInjEx_Handler, 0x1C, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)VMMCommunicationEx_Handler, 0x1D, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)SecurityEx_Handler, 0x1E, IDT_TA_InterruptGate, 0x08);

    SetIDTGate((void*)PageFault_Handler, 0xE, IDT_TA_InterruptGate, 0x08); //Page Fault
    SetIDTGate((void*)DoubleFault_Handler, 0x8, IDT_TA_InterruptGate, 0x08); //Double Fault
    SetIDTGate((void*)GPFault_Handler, 0xd, IDT_TA_InterruptGate, 0x08); //General Protection Fault
    SetIDTGate((void*)MouseInt_Handler, 0x2c, IDT_TA_InterruptGate, 0x08); //PS2 Mouse Handler
    SetIDTGate((void*)PitInt_Handler, 0x20, IDT_TA_InterruptGate, 0x08); //PIT CHIP Handler
    SetIDTGate((void*)HPETInt_Handler, 0x22, IDT_TA_InterruptGate, 0x08); //PIT CHIP Handler
    SetIDTGate((void*)APICTimerInt_ASM_Handler, 0x23, IDT_TA_InterruptGate, 0x08); //APIC TIMER Handler
    SetIDTGate((void*)PCIInt_Handler, 0x50, IDT_TA_InterruptGate, 0x08); // PCI Legacy irq interrupts
    SetIDTGate((void*)SpuriousInt_Handler, 0xFF, IDT_TA_InterruptGate, 0x08);
    SetIDTGate((void*)WM_MID_INTERRUPT, 0x7F, IDT_TA_InterruptGate_U, 0x08);

    //7A-7F reserved for userspace activities
    SetIDTGate((void*)syscall_int_handler, 0x80, IDT_TA_InterruptGate_U, 0x08); // Syscalls



    SetIDTGate((void*)SpuriousInt_Handler, 0xFF, IDT_TA_InterruptGate, 0x08);


    asm ("lidt %0" : : "m" (idtr));

    RemapPIC();
}

tss_entry_t* tss_entry;
void write_tss(BITS64_SSD *g) {
	// Compute the base and limit of the TSS for use in the GDT entry.
	uint64_t base = (uint64_t)tss_entry;
	uint32_t limit = sizeof(tss_entry_t) - 1;

	// Add a TSS descriptor to the GDT.
	g->Limit = (uint16_t)(limit & 0xFFFF);
    g->Limit2 = (uint8_t)((limit >> 16) & 0xF);
	g->Base = (uint16_t)(base & 0xFFFF);
    g->Base2 = (uint8_t)((base >> 16) & 0xFF);
    g->Base3 = (uint8_t)((base >> 24) & 0xFF);
    g->Base4 = (uint32_t)((base >> 32) & 0xFFFFFFFF);


	g->AccessByte = 0x89;
    g->Flags = 0b10;

	// Ensure the TSS is initially zero'd.
	//memset(&tss_entry, 0, sizeof(tss_entry));

	tss_entry->rsp0 = ((uint64_t)GlobalAllocator.RequestPages(10) + (0x1000 * 10));
}

extern "C" void set_kernel_stack(uint64_t stack) { // Used when an interrupt occurs
	tss_entry->rsp0 = stack;
}

void InitKernel(uint32_t magic, multiboot_info* mb_info_addr){
    InitSerial(COM1);
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC){
        serialPrint(COM1, "Could not verify bootloader magic!");
        while (1){
            __asm__ volatile ("hlt");
        }
    }

    Initialize(mb_info_addr);
    void* gdtAddress = GlobalAllocator.RequestPage();
    tss_entry = (tss_entry_t*)gdtAddress;

    GDTDescriptor* gdtDescriptor = (GDTDescriptor*)((uint64_t)tss_entry + sizeof(tss_entry_t));
    write_tss(&DefaultGDT.TSS);
    memcpy((void*)((uint64_t)gdtDescriptor + sizeof(GDTDescriptor)), &DefaultGDT, sizeof(GDT)); //Write the default gdt after the descriptor

    gdtDescriptor->Size = sizeof(GDT) - 1;
    gdtDescriptor->Offset = (uint64_t)gdtDescriptor + sizeof(GDTDescriptor);
    LoadGDT(gdtDescriptor);

    PrepareInterrupts();//-
    
    InitializeHeap((void*)0x00000F0000000000, (512 * 1024 * 1024) / 0x1000);
    globalRenderer = new BasicRenderer(fb, font);
    globalRenderer->Clear(0x0f0024);//0x5234eb

    globalRenderer->printf(0x00FF00, "[HEAP] ");
    globalRenderer->printf("Successfully allocated 0x%llx pages (%d MB)\n", HeapIntCnt, ((HeapIntCnt * 0x1000) / 1024) / 1024);
    //globalRenderer->status = false;

    outb(PIC1_DATA, 0b11111111);
    outb(PIC2_DATA, 0b11111111);
    //PIT::SetDivisor(2000);//-

    __asm__ ("sti");

    pagesz = (GlobalAllocator.GetFreeRAM() / 0x1000) / 4;
    
    xsdt = (ACPI::SDTHeader*)(((ACPI::RSDP2*)acpi->rsdp)->XSDTAddress);
    MADT();

    InitIOAPIC();
    InitLAPIC();
    HPET::Initialize();
    SetupAPICTimer();
    HPET::DisableHPET();
    /*globalRenderer->printf("Get Ready\n");
    Sleep(3000);
    globalRenderer->printf(0xFF0000, "FIRE\n");
    Sleep(10000);
    globalRenderer->printf("Stop\n");*/

    lai_set_acpi_revision(((ACPI::RSDP2*)acpi->rsdp)->Revision);
    lai_create_namespace();
    lai_enable_acpi(1); // if you use the IOAPIC

    PS2::Initialize();
    PS2Mouse::Initialize();
    PS2KB::Initialize();


    //globalRenderer->printf("X: %d, Y: %d\n", globalRenderer->targetFramebuffer->common.framebuffer_width, globalRenderer->targetFramebuffer->common.framebuffer_height);
    //globalRenderer->printf(0x000FF0, "Total system memory: %d MB\nSystem Reserved Memory: %d\nUsed Memory: %d\nFree Memory:%d\n", (GlobalAllocator.GetMemSize()/1024)/1024, (GlobalAllocator.GetReservedRAM()/1024)/1024, (GlobalAllocator.GetUsedRAM()/1024)/1024, (GlobalAllocator.GetFreeRAM()/1024)/1024);
    PrepareACPI(acpi);
}


void Shutdown(){
    lai_enter_sleep(5);
}