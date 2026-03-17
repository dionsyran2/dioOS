#pragma once
#include <stdint.h>

struct gdt_descriptor_t{
	uint16_t size;
	uint64_t offset;
} __attribute__((packed));

struct gdt_segment_descriptor_t{
	uint16_t limit;
	uint16_t base0;
	uint8_t base1;
	uint8_t access_byte;
	uint8_t flags_limit;
	uint8_t base2;
}__attribute__((packed));

struct gdt_long_system_segment_descriptor_t{
	uint16_t limit0;
	uint16_t base0;
	uint8_t base1;
	uint8_t access_byte;
	uint8_t flags_limit;
	uint8_t base3;
	uint32_t base4;
	uint32_t rsv;
}__attribute__((packed));

/* @warning RSP0 must be set (Privilage Level Change to Ring 0). All of the other fields are not used by this kernel */
struct tss_t{
	uint32_t rsv;

	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;

	uint64_t rsv2;

	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;

	uint64_t rsv3;
	uint16_t rsv4;

	uint16_t IOPB;
} __attribute__((packed));

struct gdt_t{
	gdt_segment_descriptor_t null_segment;
	gdt_segment_descriptor_t kcode;
	gdt_segment_descriptor_t kdata;

	gdt_segment_descriptor_t ucode;
	gdt_segment_descriptor_t udata;

	gdt_long_system_segment_descriptor_t tss;
} __attribute__((packed));

extern "C" void LoadGDT(void*);
void setup_gdt();