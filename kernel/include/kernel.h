#pragma once
#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <memory.h>
#include <paging/PageFrameAllocator.h>
#include <paging/PageTableManager.h>
#include <memory/heap.h>
#include <cpu.h>
#include <interrupts/IDT.h>
#include <interrupts/interrupts.h>
#include <drivers/serial.h>
#include <gdt/gdt.h>
#include <uname.h>
#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <uacpi/sleep.h>
#include <uacpi/osi.h>


// LIMINE
extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_kernel_address_request kernel_address_request;
extern volatile struct limine_module_request  module_request;
extern volatile struct limine_rsdp_request rsdp_request;

extern uint64_t _KernelStart;
extern uint64_t _KernelEnd;
extern IDTR idtr;
extern tss_entry_t* cpu0_tss_entry;
extern GDTDescriptor* gdtDescriptor;

void init_kernel();
void second_stage_init();

extern "C" void SetIDTGate(void* handler, uint8_t entryOffset, uint8_t type_attr, uint8_t selector, IDTR* idtr);
void Shutdown();
void Restart();
extern "C" void set_kernel_stack(uint64_t stack, tss_entry_t* tss_entry);