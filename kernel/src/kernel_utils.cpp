#include <kernel.h>
#include <drivers/framebuffer/framebuffer.h>
#include <devices/devices.h>
#include <devices/proc.h>
#include <random.h>

tss_entry_t* cpu0_tss_entry;
void* font;
GDTDescriptor* gdtDescriptor;

int acpi_init(void);
int power_button_init(void);

void prepare_acpi(){
    globalPTM.MapMemory((void*)physical_to_virtual((uint64_t)rsdp_request.response->address), rsdp_request.response->address);
    rsdp = (ACPI::RSDP2*)physical_to_virtual((uint64_t)rsdp_request.response->address);

    globalPTM.MapMemory((void*)physical_to_virtual(rsdp->XSDTAddress), (void*)rsdp->XSDTAddress);
    xsdt = (ACPI::SDTHeader*)physical_to_virtual(rsdp->XSDTAddress);

    mcfg = (ACPI::MCFGHeader*)ACPI::FindTable(xsdt, (char*)"MCFG");

    madt = new MADT();
}

void initialize_memory(){
    MEMORY_BASE = hhdm_request.response->offset;
    GlobalAllocator.ReadEFIMemoryMap(memmap_request.response);
    globalPTM = PageTableManager((PageTable*)GlobalAllocator.RequestPage());
    memset(globalPTM.PML4, 0, 0x1000);

    uint64_t kernel_size_in_pages = DIV_ROUND_UP(((uint64_t)&_KernelEnd - (uint64_t)&_KernelStart), 0x1000);
    GlobalAllocator.LockPages((void*)kernel_address_request.response->physical_base, kernel_size_in_pages);

    for (uint64_t i = 0; i < kernel_size_in_pages; i++){
        globalPTM.MapMemory((void*)(kernel_address_request.response->virtual_base + (i * 0x1000)),
                            (void*)(kernel_address_request.response->physical_base + (i * 0x1000)));
    }

    for (uint64_t i = 0; i < GlobalAllocator.GetMemSize(); i += 0x1000){
        globalPTM.MapMemory((void*)(MEMORY_BASE + i), (void*)i);
    }


    for (int i = 0; i < module_request.response->module_count; i++){
        limine_file* file = module_request.response->modules[i];
        size_t len = strlen(file->path);
        if (strcmp(file->path + len - 3, "psf") == 0){ // font
            font = GlobalAllocator.RequestPages(DIV_ROUND_UP(file->size + sizeof(PSF1_FONT), 0x1000));
            PSF1_FONT* f = (PSF1_FONT*)font;

            memcpy(font + sizeof(PSF1_FONT), file->address, file->size);
            f->psf1_Header = (PSF1_HEADER*)(font + sizeof(PSF1_FONT));
            f->glyphBuffer = (void*)((uint64_t)f->psf1_Header + sizeof(PSF1_HEADER));
            f->size = file->size;
            break;
        }
    }


    asm ("mov %0, %%cr3" :: "r" (virtual_to_physical((uint64_t)globalPTM.PML4)));

    InitializeHeap((void*)(MEMORY_BASE + 0x100000000000), (256 * 1024 * 1024) / 0x1000);
}


IDTR idtr;

extern "C" void SetIDTGate(void* handler, uint8_t entryOffset, uint8_t type_attr, uint8_t selector, IDTR* idtr){
    IDTDescEntry* interrupt = (IDTDescEntry*)(idtr->Offset + entryOffset * sizeof(IDTDescEntry));
    interrupt->SetOffset((uint64_t)handler);
    interrupt->type_attr = type_attr;
    interrupt->selector = selector;
    interrupt->ist = 0;
}

void PrepareInterrupts();

void init_kernel(){
    enable_nx();
    enable_simd(); // enable the fpu and any simd technology supported
    initialize_memory();

    framebuffer* fb = new framebuffer(framebuffer_request.response->framebuffers[0]);

    if (font == nullptr){
        uint32_t width, height;
        fb->get_screen_size(&width, &height, nullptr);
        fb->draw_rectangle(0, 0, width, height, 0xFF0000);
        fb->update_screen();
        while (1) __asm__ ("hlt");
    }
    
    globalRenderer = new BasicRenderer(fb, font);

    utsname* uname = get_uname();
    kprintf("%s %s, %s\n", uname->sysname, uname->release, uname->version);

    void* gdtAddress = GlobalAllocator.RequestPage();
    cpu0_tss_entry = (tss_entry_t*)gdtAddress;
    gdtDescriptor = (GDTDescriptor*)((uint64_t)cpu0_tss_entry + sizeof(tss_entry_t));
    write_tss(&DefaultGDT.TSS, cpu0_tss_entry);
    memcpy((void*)((uint64_t)gdtDescriptor + sizeof(GDTDescriptor)), &DefaultGDT, sizeof(GDT)); //Write the default gdt after the descriptor
    
    gdtDescriptor->Size = sizeof(GDT) - 1;
    gdtDescriptor->Offset = (uint64_t)gdtDescriptor + sizeof(GDTDescriptor);
    LoadGDT(gdtDescriptor);

    kprintf(0x00FF00, "[HEAP] ");
    kprintf("Successfully allocated 0x%llx pages (%d MB)\n", HeapIntCnt, ((HeapIntCnt * 0x1000) / 1024) / 1024);

    PrepareInterrupts();

    outb(PIC1_DATA, 0b11111111);
    outb(PIC2_DATA, 0b11111111);

    __asm__ ("sti");

    

    prepare_acpi();


    InitIOAPIC();
    InitLAPIC();
    HPET::Initialize();
    SetupAPICTimer();
}

void second_stage_init(){
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

    rand_init(to_unix_timestamp(c_time));

    task_t* task = task_scheduler::create_process("Teletype Terminal Emulator", (function)session::CreateSession);
    task_scheduler::mark_ready(task);
    task_scheduler::disable_scheduling = false;
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

extern "C" void set_kernel_stack(uint64_t stack, tss_entry_t* tss_entry) { // Used when an interrupt occurs
	tss_entry->rsp0 = stack;
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