#pragma once
#include "apic.h"
#include "interrupts/IDT.h"
#include <gdt/gdt.h>
#include <ArrayList.h>

extern "C" void ap_start();
extern "C" void ap_start_end();
extern "C" void ap_protected_mode_entry();


extern "C" uint32_t ap_long_mode_gdt_address;
extern "C" uint32_t ap_krnl_page_table;

extern "C" uint64_t ap_krnl_stack;

extern "C" uint64_t ap_gdt_start;
extern "C" uint64_t ap_gdt_end;

extern "C" bool ap_started;

extern bool initialized_all_aps;

extern "C" uint16_t active_cpus;

struct ApplicationProcessor{
    uint16_t CPUID;
    uint16_t APICID;
    IDTR idtr;
    tss_entry_t* tss;
};

bool StartAP(uint8_t APICID);

int StartAllAPs();

void SetupAP();

extern ArrayList<ApplicationProcessor>* ApplicationProcessors;