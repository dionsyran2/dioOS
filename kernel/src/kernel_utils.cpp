/* KERNEL UTILITIES */
#include <kernel.h>
#include <drivers/graphics/gop.h>
#include <cpu.h>
#include <acpi.h>
#include <scheduling/apic/lapic.h>
#include <scheduling/apic/ioapic.h>
#include <local.h>
#include <interrupts/interrupts.h>
#include <drivers/timers/common.h>
#include <scheduling/multiprocessor/ap_init.h>
#include <pty/pty.h>
#include <acpica/embedded_controller.h>
#include <filesystem/vfs/vfs.h>
#include <pci.h>
#include <helpers/elf.h>
#include <syscalls/syscalls.h>
#include <drivers/drivers.h>
#include <events.h>
#include <panic.h>
#include <random.h>


bool no_smp = false; // Do not enable the APs
bool is_sse_enabled;
bool is_avx_enabled;
bool is_avx2_enabled;
bool is_avx512_enabled;
size_t g_fpu_storage_size = 0;
cpu_local_data* bsp_local;
spinlock_t _cpu_local_data_chain_lock;
cpu_local_data* _last_local_data_entry;

bool kernel_has_argument(const char* arg) {
    if (kernel_file_request.response == nullptr || 
        kernel_file_request.response->kernel_file == nullptr ||
        kernel_file_request.response->kernel_file->cmdline == nullptr) {
        return false;
    }

    const char* cmdline = kernel_file_request.response->kernel_file->cmdline;

    // Quick and dirty check (strstr finds the substring anywhere)
    // For a robust OS, you'd want a proper tokenizer to avoid matching "no-smp-please" as "no-smp"
    if (strstr(cmdline, arg) != nullptr) {
        return true;
    }

    return false;
}

void initialize_memory(){
    /* Initialize the Allocator */

    MEMORY_BASE = hhdm_request.response->offset;
    bool ret = GlobalAllocator.Initialize(memmap_request.response);

    if (!ret){
        serialf("[Fatal Fault] Memory Initialization Failed!!!\n");
        while(1) __asm__ ("hlt");
    }

    /* Set up paging */

    globalPTM = PageTableManager((PageTable*)GlobalAllocator.RequestPage());
    memset(globalPTM.PML4, 0, 0x1000);
    global_ptm_cr3 = virtual_to_physical((uint64_t)globalPTM.PML4);

    uint64_t kernel_size_in_pages = DIV_ROUND_UP(((uint64_t)&_KernelEnd - (uint64_t)&_KernelStart), 0x1000);
    GlobalAllocator.LockPages((void*)kernel_address_request.response->physical_base, kernel_size_in_pages);

    for (uint64_t i = 0; i < kernel_size_in_pages; i++){
        globalPTM.MapMemory((void*)(kernel_address_request.response->virtual_base + (i * 0x1000)),
                            (void*)(kernel_address_request.response->physical_base + (i * 0x1000)));
    }

    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++){
        limine_memmap_entry* entry = memmap_request.response->entries[i];
        
        for (uint64_t i = 0; i < entry->length; i += 0x1000){
            globalPTM.MapMemory((void*)(MEMORY_BASE + i + entry->base), (void*)(i + entry->base));
        }
    }

    asm ("mov %0, %%cr3" :: "r" (global_ptm_cr3));

    /* HEAP */
    InitializeHeap(HEAP_BASE, 1 * 1024 * 1024);
}

void init_modules(){
    int psf_ext_len = strlen(PSF_EXTENSION);
    
    for (int i = 0; i < module_request.response->module_count; i++){
        limine_file* file = module_request.response->modules[i];
        int path_len = strlen(file->path);

        // Check if its a psf font
        if (path_len >= psf_ext_len && strcmp(file->path + (path_len - psf_ext_len), PSF_EXTENSION) == 0){            
            // Initialize the renderer
            init_psf_renderer(file);
        }
    }
}

char* dev_tty_read_link(vnode_t* this_node){
    task_t *self = task_scheduler::get_current_task();

    char *path = nullptr;

    if (self && self->ctty){
        path = vfs::get_full_path_name(self->ctty);
    } else {
        char *str = "/dev/tty0";
        
        path = (char *)malloc(strlen(str) + 1);
        strcpy(path, str);
    }

    return path;
}


void init_display(){
    if (framebuffer_request.response == nullptr || framebuffer_request.response->framebuffers == nullptr) return;

    // Create a driver
    drivers::GraphicsDriver* fb = new drivers::GOP(framebuffer_request.response->framebuffers[0]);

    // Create a vt
    main_vt = new virtual_terminal(fb);

    kprintf("dioOS build %s\n", __DATE__);

    vnode_t *tty = vfs::create_path("/dev/tty", VLNK);

    tty->file_operations.read_link = dev_tty_read_link;
    tty->close();
}

void init_acpi(){
    rsdp = (ACPI::RSDP2*)physical_to_virtual((uint64_t)rsdp_request.response->address);
    xsdt = (ACPI::SDTHeader*)physical_to_virtual(rsdp->XSDTAddress);
}

void check_boot_arguments(){
    no_smp = kernel_has_argument("no-smp");
}

void setup_exception_handlers(){
    _set_interrupt_service_routine((void*)isr0, 0, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr1, 1, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr2, 2, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr3, 3, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr4, 4, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr5, 5, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr6, 6, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr7, 7, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr8, 8, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr9, 9, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr10, 10, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr11, 11, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr12, 12, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr13, 13, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr14, 14, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr16, 16, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr17, 17, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr18, 18, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr19, 19, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr20, 20, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)coprocessor_halt_execution, HALT_EXEC_INTERRUPT_VECTOR, IDT_TA_InterruptGate, 0x08);
}

void setup_bsp_interrupts(){
    setup_exception_handlers();
    _set_interrupt_service_routine((void*)spurious_interrupt_handler, SPURIOUS_INTERRUPT_VECTOR, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)task_scheduler_swap_task, SCHEDULER_SWAP_TASKS_VECTOR, IDT_TA_InterruptGate, 0x08);
}

void setup_ap_interrupts(){
    setup_exception_handlers();
    _set_interrupt_service_routine((void*)spurious_interrupt_handler, SPURIOUS_INTERRUPT_VECTOR, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)task_scheduler_swap_task, SCHEDULER_SWAP_TASKS_VECTOR, IDT_TA_InterruptGate, 0x08);
}

void ap_setup(){
    _enable_sse();
    _enable_avx();
    _enable_avx512();
    enable_nx();
    setup_syscalls();

    init_core();
    setup_ap_interrupts();

    asm ("sti");
    
    cpu_local_data* local = get_cpu_local_data();
    local->apic_timer_initialized = APIC_TIMER::initialize_local_apic_timer();
    TSC::calibrate_tsc();

    task_scheduler::initialize_scheduler();

    local->disable_scheduling = false;
    while(1) asm("hlt");
}

void init_kernel_subsystems();

void init_kernel(){
    is_sse_enabled = _enable_sse();
    is_avx_enabled = _enable_avx();
    is_avx2_enabled = _cpu_has_avx2() && is_avx_enabled;
    is_avx512_enabled = _enable_avx512();
    detect_fpu_area_size();
    enable_nx();

    initialize_memory();
    init_modules();
    
    // Initialize the vfs layer
    vfs::initialize_vfs();

    init_display();
    check_boot_arguments();
    
    if (!is_sse_enabled) kprintf("\e[0;33m[CPU]\e[0m SSE extention not supported!\n");
    if (!is_avx_enabled) kprintf("\e[0;33m[CPU]\e[0m AVX extention not supported!\n");
    if (!is_avx2_enabled) kprintf("\e[0;33m[CPU]\e[0m AVX2 extention not supported!\n");
    if (!is_avx512_enabled) kprintf("\e[0;33m[CPU]\e[0m AVX512 extention not supported!\n");
    
    init_acpi();
    init_io_apic(); // Note: All external interrupts go directly to the bsp!!!
    setup_syscalls();

    init_core();
    bsp_local = get_cpu_local_data();
    
    setup_bsp_interrupts();
    asm ("sti");
    
    initialize_timers();

    rand_init(current_time);
    if (!no_smp) start_all_aps();

    kprintf("\e[0;32m[INFO]\e[0m Total active CPUs: %d\n", local_cpu_cnt);
    
    task_t *init = task_scheduler::create_process("Kernel Subsystem Initialization", init_kernel_subsystems, false, false);
    init->affinity = 1; // Only BSP
    task_scheduler::mark_ready(init);
}

void acpi_set_apic_mode() {
    ACPI_OBJECT_LIST arg_list;
    ACPI_OBJECT arg[1];

    arg_list.Count = 1;
    arg_list.Pointer = arg;
    arg[0].Type = ACPI_TYPE_INTEGER;
    arg[0].Integer.Value = 1;

    ACPI_STATUS status = AcpiEvaluateObject(ACPI_ROOT_OBJECT, (ACPI_STRING)"_PIC", &arg_list, NULL);
    
    if (ACPI_SUCCESS(status)) {
        kprintf("[ACPI] Switched to IOAPIC mode via _PIC\n");
    } else {
        kprintf("[ACPI] _PIC method not found or failed: %s\n", AcpiFormatException(status));
    }
}

void InitializeACPICA() {
    ACPI_STATUS status;

    kprintf("Initializing ACPI...\n");

    status = AcpiInitializeSubsystem();
    if (ACPI_FAILURE(status)) {
        kprintf("AcpiInitializeSubsystem failed: %s\n", AcpiFormatException(status));
        return;
    }

    status = AcpiInitializeTables(NULL, 16, FALSE);
    if (ACPI_FAILURE(status)) {
        kprintf("AcpiInitializeTables failed: %s\n", AcpiFormatException(status));
        return;
    }

    status = AcpiLoadTables();
    if (ACPI_FAILURE(status)) {
        kprintf("AcpiLoadTables failed: %s\n", AcpiFormatException(status));
        return;
    }

    
    status = acpi_intialize_embedded_controller();
    if (ACPI_FAILURE(status)) {
        kprintf("Failed to install EC handler: %s\n", AcpiFormatException(status));
    }
    
    status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(status)) {
        kprintf("AcpiEnableSubsystem failed: %s\n", AcpiFormatException(status));
    }

    status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(status)) {
        kprintf("AcpiInitializeObjects failed: %s\n", AcpiFormatException(status));
    }

    acpi_set_apic_mode();

    embedded_controler_enable_gpes();

    setup_acpi_kernel_events();

    AcpiUpdateAllGpes();
    
    kprintf("ACPICA Initialized Successfully!\n");

    return;
}

char* init_executables[] = {
    "/sbin/init",
    "/etc/init",
    "/bin/init",
    "/bin/sh",
    nullptr
};

void start_userspace(){
    vnode_t* fnode = nullptr;

    int i = 0;
    while (init_executables[i] != nullptr && fnode == nullptr){
        fnode = vfs::resolve_path(init_executables[i]);
        if (fnode == nullptr) i++;
    }

    if (fnode == nullptr) panic("Init executable could not be found!", "");

    char* args[] = {init_executables[i], nullptr};
        
    task_t* task = execute_elf(fnode, 1, args, init_executables[i]);
    task->tgid = 1;
    task->pid = 1;
    task->ctty = vfs::resolve_path("/dev/tty0");

    task_scheduler::mark_ready(task);
}
void init_kernel_subsystems(){
    task_t* self = task_scheduler::get_current_task();

    // Initialize ACPI
    InitializeACPICA();

    pci::enumerate_pci();

    start_drivers();

    log_memory_usage();

    start_userspace();
    self->exit(0);
}
