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
#include <lai/helpers/sci.h>
#include <lai/helpers/pm.h>
#include <lai/core.h>
#include "scheduling/hpet/hpet.h"
#include "scheduling/task/scheduler.h"
#include "gdt/gdt.h"
#include "userspace/userspace.h"
#include "syscalls/syscalls.h"
#include "UserInput/ps2.h"
#include "UserInput/keyboard.h"
#include "UserInput/mouse.h"

//Drivers
#include "drivers/serial.h"
#include "drivers/audio/HDA.h"
#include "filesystem/FAT32.h"
#include "drivers/audio/decoding/mp3.h"
#include "drivers/storage/volume/volume.h"
#include "other/ELFLoader.h"
#include "drivers/rtc/rtc.h"

//Defines

struct multiboot_info {
    uint32_t size;
    uint32_t reserved;
    struct multiboot_tag* first;
};


extern uint64_t _KernelStart;
extern uint64_t _KernelEnd;
void InitKernel(uint32_t magic, multiboot_info* mb_info_addr);
void Shutdown();
extern "C" void SetIDTGate(void* handler, uint8_t entryOffset, uint8_t type_attr, uint8_t selector);
extern "C" void set_kernel_stack(uint64_t stack);
extern "C" void _main(uint32_t magic, multiboot_info* mb_info_addr);
extern uint64_t stack_top_usr;
extern uint64_t stack_top;