#include <kernel.h>
#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <uacpi/sleep.h>
#include <uacpi/osi.h>
#include <kerrno.h>
#include <random.h>
#include <devices/devices.h>
#include <devices/proc.h>

struct multiboot_tag_framebuffer* fb = nullptr;
struct multiboot_tag_new_acpi* acpi;
bool RendererStatus = false;
void PrepareACPI(multiboot_tag_new_acpi* acpi){
    rsdp = (ACPI::RSDP2*)acpi->rsdp;
    globalPTM.MapMemory((void*)rsdp, (void*)rsdp);
    
    mcfg = (ACPI::MCFGHeader*)ACPI::FindTable(xsdt, (char*)"MCFG");
    globalPTM.MapMemory((void*)mcfg, (void*)mcfg);
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

    size_t entryCount = (mmap->size - sizeof(multiboot_tag_efi_mmap)) / mmap->descr_size;
    for (uint64_t t = 0; t < GetMemorySize((EFI_MEMORY_DESCRIPTOR*)mmap->efi_mmap, entryCount, mmap->descr_size); t+= 0x1000){
        globalPTM.MapMemory((void*)t, (void*)t); // Map the entire memory 1:1
    }

    /*size_t mMapEntries = (mmap->size - sizeof(multiboot_tag_efi_mmap)) / mmap->descr_size;
    //Map all of the conventional memory
    for (int i = 0; i < mMapEntries; i++){
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)mmap->efi_mmap + (i * mmap->descr_size));
        if (1/*desc->type == 7 || desc->type == 2 || desc->type == 4 || desc->type == 5 || desc->type == 6 || desc->type == 3 || desc->type == 9*//*){
            for (int o = 0; o < desc->numPages; o++){
                globalPTM.MapMemory((void*)((uint64_t)desc->physAddr + (o * 0x1000)), (void*)((uint64_t)desc->physAddr + (o * 0x1000)));
            }
        }
    }*/

    
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

bool cmdline_has_flag(const char* cmdline, const char* key) {
    char* p = (char*)cmdline;
    size_t keylen = strlen(key) - 1;
    size_t len = strlen(cmdline);

    if (len >= keylen && strncmp(p, key, keylen) == 0) {
        return true;
    }
    return false;
}




void Initialize(multiboot_info* bootInfo){
    struct PSF1_FONT psf1_font;
    struct multiboot_tag_module* psf1_module;
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
                psf1_module = module;
                break;
            }
            case MULTIBOOT_TAG_TYPE_ACPI_NEW:{
                acpi = (struct multiboot_tag_new_acpi*)tag;
                break;
            }
            case MULTIBOOT_TAG_TYPE_CMDLINE:{
                char* cmdline = ((struct multiboot_tag_string*) tag)->string;
                /*serialPrint(COM1, cmdline);
                serialPrint(COM1, toString((uint8_t)cmdline_has_flag(cmdline, "debug")));*/
                if (cmdline_has_flag(cmdline, "debug")){
                    RendererStatus = true;
                }else{
                    RendererStatus = false;
                }
            }
            default: {
                //serialPrint(COM1, toString(tag->type));
                //serialPrint(COM1, "-");
                break;
            }
        }

        add_size = tag->size;

        // Align the size to 8 bytes.
        if ((add_size % 8) != 0)
			add_size += (8 - add_size % 8);
    }

    GlobalAllocator.LockPages((void*)module->mod_start, (module->mod_end - module->mod_start) / 0x1000);
    GlobalAllocator.LockPages((void*)psf1_module->mod_start, (psf1_module->mod_end - psf1_module->mod_start) / 0x1000);
    font = (PSF1_FONT*)malloc(sizeof(PSF1_FONT));
    font->psf1_Header = psf1_font.psf1_Header;
    font->glyphBuffer = psf1_font.glyphBuffer;
}

IDTR idtr;

extern "C" void SetIDTGate(void* handler, uint8_t entryOffset, uint8_t type_attr, uint8_t selector, IDTR* idtr){
    IDTDescEntry* interrupt = (IDTDescEntry*)(idtr->Offset + entryOffset * sizeof(IDTDescEntry));
    interrupt->SetOffset((uint64_t)handler);
    interrupt->type_attr = type_attr;
    interrupt->selector = selector;
    interrupt->ist = 0;
}

void PrepareInterrupts(){
    idtr.Limit = 0x0FFF;
    idtr.Offset = (uint64_t)GlobalAllocator.RequestPage();
    memset((void*)idtr.Offset, 0, 0x1000);

    SetIDTGate((void*)HLT_IPI_Handler, IPI_VECTOR_HALT_EXECUTION, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)DivisionError_Handler, 0x0, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)BoundRangeExceed_Handler, 0x5, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)InvalidOpcode_Handler, 0x6, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)DeviceNotAvailable_Handler, 0x7, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)InvalidTSS_Handler, 0xA, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)SegmentNotPresent_Handler, 0xB, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)StackSegFault_Handler, 0xC, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)x87FloatingPointEx_Handler, 0x10, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)AlignementCheck_Handler, 0x11, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)SIMDFloatPointEx_Handler, 0x13, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)VirtualizationEx_Handler, 0x14, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)CPEx_Handler, 0x15, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)HypervisorInjEx_Handler, 0x1C, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)VMMCommunicationEx_Handler, 0x1D, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)SecurityEx_Handler, 0x1E, IDT_TA_InterruptGate, 0x08, &idtr);

    SetIDTGate((void*)PageFault_Handler, 0xE, IDT_TA_InterruptGate, 0x08, &idtr); //Page Fault
    SetIDTGate((void*)DoubleFault_Handler, 0x8, IDT_TA_InterruptGate, 0x08, &idtr); //Double Fault
    SetIDTGate((void*)GPFault_Handler, 0xd, IDT_TA_InterruptGate, 0x08, &idtr); //General Protection Fault
    SetIDTGate((void*)MouseInt_Handler, 0x2c, IDT_TA_InterruptGate, 0x08, &idtr); //PS2 Mouse Handler
    SetIDTGate((void*)PitInt_Handler, 0x20, IDT_TA_InterruptGate, 0x08, &idtr); //PIT CHIP Handler
    SetIDTGate((void*)HPETInt_Handler, 0x22, IDT_TA_InterruptGate, 0x08, &idtr); //HPET Handler
    SetIDTGate((void*)APICTimerInt_ASM_Handler, 0x23, IDT_TA_InterruptGate, 0x08, &idtr); //APIC TIMER Handler
    SetIDTGate((void*)PCIInt_Handler, PCI_INT_HANDLER_VECTOR, IDT_TA_InterruptGate, 0x08, &idtr); // PCI Legacy irq interrupts
    SetIDTGate((void*)SpuriousInt_Handler, 0xFF, IDT_TA_InterruptGate, 0x08, &idtr);


    //7A-7F reserved for userspace activities
    //SetIDTGate((void*)syscall_int_handler, 0x80, IDT_TA_InterruptGate_U, 0x08, &idtr); // Syscalls



    SetIDTGate((void*)SpuriousInt_Handler, 0xFF, IDT_TA_InterruptGate, 0x08, &idtr);


    asm ("lidt %0" : : "m" (idtr));

    RemapPIC();
}

tss_entry_t* cpu0_tss_entry;
void write_tss(BITS64_SSD *g, tss_entry_t* tss_entry) {
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

	tss_entry->rsp0 = ((uint64_t)GlobalAllocator.RequestPages((4 * 1024 * 1024) / 0x1000) + (4 * 1024 * 1024));
}

extern "C" void set_kernel_stack(uint64_t stack, tss_entry_t* tss_entry) { // Used when an interrupt occurs
	tss_entry->rsp0 = stack;
}

void enable_nx() {
    uint64_t efer = read_msr(IA32_EFER);
    efer |= IA32_EFER_NXE;
    write_msr(IA32_EFER, efer);
}

GDTDescriptor* gdtDescriptor;
void InitKernel(uint32_t magic, multiboot_info* mb_info_addr){
    enable_nx();
    InitSerial(COM1);
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC){
        serialPrint(COM1, "Could not verify bootloader magic!");
        while (1){
            __asm__ volatile ("hlt");
        }
    }
    serialPrint(COM1, "\n\r");

    Initialize(mb_info_addr);
    void* gdtAddress = GlobalAllocator.RequestPage();
    cpu0_tss_entry = (tss_entry_t*)gdtAddress;

    gdtDescriptor = (GDTDescriptor*)((uint64_t)cpu0_tss_entry + sizeof(tss_entry_t));
    write_tss(&DefaultGDT.TSS, cpu0_tss_entry);
    memcpy((void*)((uint64_t)gdtDescriptor + sizeof(GDTDescriptor)), &DefaultGDT, sizeof(GDT)); //Write the default gdt after the descriptor

    gdtDescriptor->Size = sizeof(GDT) - 1;
    gdtDescriptor->Offset = (uint64_t)gdtDescriptor + sizeof(GDTDescriptor);
    LoadGDT(gdtDescriptor);

    PrepareInterrupts();//-
    
    InitializeHeap((void*)0xFFFF800000000000, (128 * 1024 * 1024) / 0x1000);
    globalRenderer = new BasicRenderer(fb, font);
    globalRenderer->Set(RendererStatus);
    globalRenderer->Clear(0);//0x0f0024//0x5234eb

    kprintf(0x00FF00, "[HEAP] ");
    kprintf("Successfully allocated 0x%llx pages (%d MB)\n", HeapIntCnt, ((HeapIntCnt * 0x1000) / 1024) / 1024);
    //globalRenderer->status = false;

    outb(PIC1_DATA, 0b11111111);
    outb(PIC2_DATA, 0b11111111);
    //PIT::SetDivisor(2000);//-

    __asm__ ("sti");
    rsdp = (ACPI::RSDP2*)acpi->rsdp;
    xsdt = (ACPI::SDTHeader*)(rsdp->XSDTAddress);


    madt = new MADT();
    
    InitIOAPIC();
    InitLAPIC();
    HPET::Initialize();
    SetupAPICTimer();
    //HPET::DisableHPET();

    //scheduler::Init();
    //StartAllAPs();
    /*
    * I was working on smp support but i cannot get the damn scheduler to work...
    * i will fix it some other day...
    */
}

int acpi_init(void);

int power_button_init(void);

void SecondaryKernelInit(){
    PrepareACPI(acpi);

    acpi_init();
    power_button_init();


    PS2::Initialize();
    PS2Mouse::Initialize();
    PS2KB::Initialize();

    PCI::EnumeratePCI(mcfg);
    
    init_vfs_dev();
    init_proc_fs();
    
    if(!load_users()){
        panic("Could not locate the user data!", "Make sure the root filesystem contains the necessary files!");
    }

    if (!globalRenderer->status){
        boolean_flag_1 = true;
        while(boolean_flag_1);
    }

    rand_init(to_unix_timestamp(c_time->second, c_time->minute, c_time->hour, c_time->day, c_time->month, c_time->year));


    task_t* task = task_scheduler::create_process("Teletype Terminal Emulator", (function)session::CreateSession);
    globalRenderer->Set(false);
    task_scheduler::mark_ready(task);
}


int acpi_init(void) {
    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret)) {
        kprintf(0xFF0000, "[uACPI] ");
        kprintf("uacpi_initialize error: %s\n", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret)) {
        kprintf(0xFF0000, "[uACPI] ");
        kprintf("uacpi_namespace_load error: %s\n", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret)) {
        kprintf(0xFF0000, "[uACPI] ");
        kprintf("uacpi_namespace_initialize error: %s\n", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret)) {
        kprintf(0xFF0000, "[uACPI] ");
        kprintf("uACPI GPE initialization error: %s\n", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    return 0;
}

static uacpi_interrupt_ret handle_power_button(uacpi_handle ctx) {
    Shutdown();
    return UACPI_INTERRUPT_HANDLED;
}

int power_button_init(void) {
    uacpi_status ret = uacpi_install_fixed_event_handler(
        UACPI_FIXED_EVENT_POWER_BUTTON,
	    handle_power_button, UACPI_NULL
    );
    
    if (uacpi_unlikely_error(ret)) {
        kprintf(0xFF0000, "[uACPI] ");
        kprintf("failed to install power button event callback: %s\n", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    return 0;
}


void Shutdown(){
        /*
     * Prepare the system for shutdown.
     * This will run the \_PTS & \_SST methods, if they exist, as well as
     * some work to fetch the \_S5 and \_S0 values to make system wake
     * possible later on.
     */
    kprintf(0xFF0000, "Shutting down!");
    //taskScheduler::disableSwitch = true;
    
    uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret)) {
        kprintf("failed to prepare for sleep: %s", uacpi_status_to_string(ret));
        return;
    }

    /*
     * This is where we disable interrupts to prevent anything from
     * racing with our shutdown sequence below.
     */
    asm ("cli");

    /*
     * Actually do the work of entering the sleep state by writing to the hardware
     * registers with the values we fetched during preparation.
     * This will also disable runtime events and enable only those that are
     * needed for wake.
     */
    ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret)) {
        kprintf("failed to enter sleep: %s", uacpi_status_to_string(ret));
        return;
    }

    return;
}

void Restart(){
    uacpi_reboot();
}