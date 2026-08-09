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
#include <pci.h>
#include <drivers/drivers.h>
#include <panic.h>
#include <random.h>
#include <network/common/common.h>
#include <rendering/multiplexer.h>
#include <vfs/vfs.h>
#include <elf/elf.h>



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

    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++){
        limine_memmap_entry* entry = memmap_request.response->entries[i];
        
        for (uint64_t i = 0; i < entry->length; i += 0x1000){
            globalPTM.MapMemory((void*)(MEMORY_BASE + i + entry->base), (void*)(i + entry->base));
            globalPTM.SetFlag((void*)(MEMORY_BASE + i + entry->base), PT_Flag::NX, true);
        }
    }

    for (uint64_t i = 0; i < kernel_size_in_pages; i++){
        globalPTM.MapMemory((void*)(kernel_address_request.response->virtual_base + (i * 0x1000)),
                            (void*)(kernel_address_request.response->physical_base + (i * 0x1000)));
    }

    asm ("mov %0, %%cr3" :: "r" (global_ptm_cr3));

    /* HEAP */
    InitializeHeap(HEAP_BASE, 1 * 1024 * 1024);
}


#include <drivers/filesystems/tarfs/tarfs.h>

void init_modules(){
    int psf_ext_len = strlen(PSF_EXTENSION);
    
    for (int i = 0; i < module_request.response->module_count; i++){
        limine_file* file = module_request.response->modules[i];
        int path_len = strlen(file->path);

        tarfs::init_tarfs(file->address, file->size);
    }
}


void init_display(){
    // Initialize the font
    vnode_t *font = vfs::resolve_path("/fonts/vga16.psf");
    if (font){
        void *buffer = malloc(font->size);
        font->read(buffer, font->size, 0);

        init_psf_renderer(buffer, font->size);
    } else {
        serialf("No font file found!\n");
    }
    
    if (framebuffer_request.response == nullptr || framebuffer_request.response->framebuffers == nullptr) {
        serialf("No framebuffer found\n");
        return;
    }

    // Create a driver
    drivers::GraphicsDriver* fb = new drivers::GOP(framebuffer_request.response->framebuffers[0]);

    global_multiplexer = new vt_multiplexer(fb, 8);
    
    global_multiplexer->select_vt(0);
}

void init_acpi(){
    rsdp = (ACPI::RSDP2*)physical_to_virtual((uint64_t)rsdp_request.response->address);
    xsdt = (ACPI::SDTHeader*)physical_to_virtual(rsdp->XSDTAddress);
}

void check_boot_arguments(){
    no_smp = kernel_has_argument("no-smp");
}


void ap_setup(){
    _enable_sse();
    _enable_avx();
    _enable_avx512();
    init_core();
    setup_ap_interrupts();

    asm ("sti");
    
    cpu_local_data* local = get_cpu_local_data();
    local->apic_timer_initialized = APIC_TIMER::initialize_local_apic_timer();
    TSC::calibrate_tsc();

    task_scheduler::initialize_core();

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

    init_core();
    bsp_local = get_cpu_local_data();
    task_scheduler::initialize_core();

    vfs::initialize(); // Initialize the vfs

    init_modules();

    init_display();

    check_boot_arguments();
    
    if (!is_sse_enabled) kprintf("\e[0;33m[CPU]\e[0m SSE extention not supported!\n");
    if (!is_avx_enabled) kprintf("\e[0;33m[CPU]\e[0m AVX extention not supported!\n");
    if (!is_avx2_enabled) kprintf("\e[0;33m[CPU]\e[0m AVX2 extention not supported!\n");
    if (!is_avx512_enabled) kprintf("\e[0;33m[CPU]\e[0m AVX512 extention not supported!\n");
    
    init_acpi();
    init_io_apic(); // Note: All external interrupts go directly to the bsp!!!
    setup_bsp_interrupts();
    asm ("sti");
    InitSerial();
    
    initialize_timers();

    rand_init(current_time);
    if (!no_smp) start_all_aps();

    kprintf("\e[0;32m[INFO]\e[0m Total active CPUs: %d\n", local_cpu_cnt);

    task_t *task = task_scheduler::create_process("k_init", init_kernel_subsystems, false);
    task_scheduler::mark_as_ready(task);
}




char* init_executables[] = {
    "/ssbin/init",
    "/etc/init",
    "/bin/init",
    "/bin/sh",
    nullptr
};

void start_userspace(){
    vnode_t *node = vfs::resolve_path("/temp/test");

    task_t *init = task_scheduler::create_process("init", nullptr, true, true);

    char *argp[] = {
        "/temp/test",
        nullptr
    };

    load_elf(init, node, 1, argp, "/temp/test");
    
    task_scheduler::mark_as_ready(init);
}

void dump_fs(int indent, vnode_t *node){
    dentry_t *out;
    size_t count = node->get_listing(out, 0, 1000);

    for (int i = 0; i < count; i++){
        for (int o = 0; o < indent; o++){
            kprintf("%c", o == (indent - 1) ? '-' : ' ');
        }
        
        vnode_t *vnode = out[i].fetch_vnode();

        if (!vnode) continue;

        kprintf("%s | %d Bytes | %o / %d\n", out[i].name, vnode->size, vnode->attributes.mode, S_ISDIR(vnode->attributes.mode));

        if (S_ISDIR(vnode->attributes.mode)){
            dump_fs(indent + 2, vnode);
        }
        
        delete vnode;
    }

    delete[] out;
}

#include <drivers/filesystems/devfs/devfs.h>
void init_kernel_subsystems(){
    task_t *self = task_scheduler::get_current_task();
    vnode_t *r = vfs::resolve_path("/");
    dump_fs(0, r);
    r->close();

    // Initialize ACPI
    ACPI::InitializeACPICA();

    pci::enumerate_pci();

    start_drivers();

    log_memory_usage();

    start_userspace();

    self->exit(0);
}
