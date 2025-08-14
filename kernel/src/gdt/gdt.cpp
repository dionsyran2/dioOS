#include <gdt/gdt.h>
#include <paging/PageFrameAllocator.h>

GDT DefaultGDT = {
    {0, 0, 0, 0x00, 0x00, 0}, // Null segment
    {0xFFFF, 0, 0, 0x9a, 0xAF, 0}, // Kernel code segment (DPL 0)
    {0xFFFF, 0, 0, 0x92, 0xCF, 0}, // Kernel data segment (DPL 0)

    // DID I MENTION HOW MUCH I HATE THOSE SYSRET SCHENANIGANS
    {0xFFFF, 0, 0, 0xF2, 0xCF, 0},        // User data segment (DPL 3)
    {0xFFFF, 0, 0, 0xFA, 0xAF, 0},        // User code segment (DPL 3)
    {0, 0, 0, 0, 0, 0, 0, 0, 0}, //Set programmatically
};

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
