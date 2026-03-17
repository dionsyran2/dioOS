/* This is the main kernel header file. It contains important definitions */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* MEMORY */
#include <memory.h>
#include <paging/PageFrameAllocator.h>
#include <paging/PageTableManager.h>
#include <memory/heap.h>

/* Graphics */
#include <rendering/psf.h>
#include <rendering/vt.h>

/* Other Stuff*/
#include <cstr.h>
#include <drivers/timers/common.h>
#include <CONFIG.h>

#ifdef BUILTIN_DEBUGGER
#include <minidbg/x86_64/dbg.h>
#endif

/* DRIVERS */
#include <drivers/serial/serial.h>

/* LIMINE SCHENANIGANS (Requests) */

extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_kernel_address_request kernel_address_request;
extern volatile struct limine_module_request  module_request;
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_kernel_file_request kernel_file_request;

/* LINKER */
extern uint64_t _KernelStart;
extern uint64_t _KernelEnd;

/* UTILS */

// @brief Initializes the kernel
void init_kernel();