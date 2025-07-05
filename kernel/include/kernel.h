#pragma once
#include <stdint.h>
#include <stddef.h>
#include "cstr.h"
#include "multiboot2.h"
#include "paging/PageFrameAllocator.h"
#include "paging/PageTableManager.h"
#include "SimpleFont.h"
#include "BasicRenderer.h"
#include "acpi.h"
#include "pci.h"
#include "memory/heap.h"
#include "scheduling/apic/apic.h"
#include "interrupts/IDT.h"
#include "interrupts/interrupts.h"
#include "scheduling/apic/madt.h"
#include "scheduling/hpet/hpet.h"
#include "scheduling/task/scheduler.h"
#include "gdt/gdt.h"
#include "syscalls/syscalls.h"
#include <cpu.h>
#include <users.h>

//Drivers
#include "drivers/serial.h"
#include "drivers/audio/HDA.h"
#include "drivers/audio/decoding/mp3.h"
#include "other/ELFLoader.h"
#include "drivers/rtc/rtc.h"
#include "UserInput/ps2.h"
#include "UserInput/keyboard.h"
#include "UserInput/mouse.h"
#include <tty/tty.h>
#include <session/session.h>

//Defines

struct multiboot_info {
    uint32_t size;
    uint32_t reserved;
    struct multiboot_tag* first;
};

extern uint64_t _KernelStart;
extern uint64_t _KernelEnd;
void InitKernel(uint32_t magic, multiboot_info* mb_info_addr);
void SecondaryKernelInit();
void Shutdown();
void Restart();
extern "C" void SetIDTGate(void* handler, uint8_t entryOffset, uint8_t type_attr, uint8_t selector, IDTR* idtr);
extern "C" void set_kernel_stack(uint64_t stack, tss_entry_t* tss_entry);
extern "C" void _main(uint32_t magic, multiboot_info* mb_info_addr);
void write_tss(BITS64_SSD *g, tss_entry_t* tss_entry);
extern uint64_t stack_top_usr;
extern uint64_t stack_top;
extern bool boolean_flag_1;
extern tss_entry_t* cpu0_tss_entry;
extern GDTDescriptor* gdtDescriptor;
extern "C" void _apmain();
extern IDTR idtr;