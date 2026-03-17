#include <gdt/gdt.h>
#include <paging/PageFrameAllocator.h>
#include <memory.h>
#include <local.h>

gdt_t default_gdt = {
    {0, 0, 0, 0x00, 0x00, 0},               // Null segment
    {0xFFFF, 0, 0, 0x9a, 0xAF, 0},          // Kernel code segment (DPL 0)
    {0xFFFF, 0, 0, 0x92, 0xCF, 0},          // Kernel data segment (DPL 0)

    {0xFFFF, 0, 0, 0xFA, 0xAF, 0},          // User code segment (DPL 3)
    {0xFFFF, 0, 0, 0xF2, 0xCF, 0},           // User data segment (DPL 3)
    { 0 }
};

void setup_gdt(){
    cpu_local_data* local = get_cpu_local_data();

    if (!local || local != local->self_reference) return kprintf("\e[0;31m[SETUP_GDT]\e[0m The Core Local Data structure is pointing to an invalid memory region!\n");

    local->global_descriptor_table = (gdt_t*)GlobalAllocator.RequestPage();
    memcpy(local->global_descriptor_table, &default_gdt, sizeof(gdt_t));

    local->tss = (tss_t*)((uint64_t)local->global_descriptor_table + sizeof(gdt_t));
    memset(local->tss, 0, sizeof(tss_t));

    uint64_t tss_base = (uint64_t)local->tss;
    uint32_t tss_limit = sizeof(tss_t) - 1;

    // Base Address splitting
    local->global_descriptor_table->tss.base0 = tss_base & 0xFFFF;
    local->global_descriptor_table->tss.base1 = (tss_base >> 16) & 0xFF;
    local->global_descriptor_table->tss.base3 = (tss_base >> 24) & 0xFF;
    local->global_descriptor_table->tss.base4 = (tss_base >> 32) & 0xFFFFFFFF;

    // Limit Splitting
    local->global_descriptor_table->tss.limit0 = tss_limit & 0xFFFF;
    local->global_descriptor_table->tss.flags_limit = (tss_limit >> 16) & 0x0F;

    // Access Byte & Flags
    // Type 0x9 = 64-bit TSS (Available)
    // Present = 1, DPL = 0
    // 0x89 = 1000 1001
    local->global_descriptor_table->tss.access_byte = 0x89;

    gdt_descriptor_t* gdt_desc = (gdt_descriptor_t*)((uint64_t)local->global_descriptor_table + sizeof(gdt_t) + sizeof(tss_t));
    gdt_desc->size = sizeof(gdt_t) - 1;
    gdt_desc->offset = (uint64_t)local->global_descriptor_table;

    // Load the TSS (Task Register)
    // The TSS is the 6th entry (index 5).
    // Indices: Null(0), KCode(1), KData(2), UCode(3), UData(4), TSS(5)
    // Selector = Index * 8 = 5 * 8 = 40 (0x28)
    // Bottom 2 bits (RPL) must be 0 for LTR instruction
    LoadGDT(gdt_desc);
    asm volatile("ltr %%ax" : : "a"(0x28));
}