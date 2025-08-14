#pragma once
#include <stdint.h>

struct tss_entry_struct {
	uint32_t rsv;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t rsv2;
	uint64_t IST1;
	uint64_t IST2;
	uint64_t IST3;
	uint64_t IST4;
	uint64_t IST5;
	uint64_t IST6;
	uint64_t IST7;
	uint64_t rsv3;
	uint16_t rsv4;
	uint16_t IOPB;

} __attribute__((packed));

typedef struct tss_entry_struct tss_entry_t;
struct GDTDescriptor {
    uint16_t Size;
    uint64_t Offset;
} __attribute__((packed));

struct GDTEntry {
    uint16_t Limit0;
    uint16_t Base0;
    uint8_t Base1;
    uint8_t AccessByte;
    uint8_t Limit1_Flags;
    uint8_t Base2;
}__attribute__((packed));

struct BITS64_SSD{
	uint16_t Limit;
	uint16_t Base;
	uint8_t Base2;
	uint8_t AccessByte;
	uint8_t Limit2:4;
	uint8_t Flags:4;
	uint8_t Base3;
	uint32_t Base4;
	uint32_t rsv;
} __attribute__((packed));

struct GDT {
    GDTEntry Null; //0x00
    GDTEntry KernelCode; //0x08
    GDTEntry KernelData; //0x10
    GDTEntry UserCode;
    GDTEntry UserData;
    BITS64_SSD TSS;
} __attribute__((packed)) 
__attribute((aligned(0x1000)));



extern GDT DefaultGDT;


extern "C" void LoadGDT(GDTDescriptor* gdtDescriptor);
void write_tss(BITS64_SSD *g, tss_entry_t* tss_entry);