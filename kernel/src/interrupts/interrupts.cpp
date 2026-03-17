#include <interrupts/interrupts.h>
#include <local.h>
#include <drivers/serial/serial.h>
#include <kstdio.h>
#include <cpu.h>
#include <drivers/timers/common.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <memory.h>

uint64_t FreeVector = FIRST_FREE_VECTOR;


__attribute__((interrupt)) void coprocessor_halt_execution(void*){
    asm ("cli");
    asm ("hlt");
    while(1) asm ("hlt"); // unreachable
}

extern "C" void spurious_interrupt_handler_cpp(void*){
    EOI();
    return;
}

extern "C" void apic_timer_handler_cpp(__registers_t* regs){
    uint64_t old_cr3 = 0;
    asm volatile ("mov %%cr3, %0" : "=r" (old_cr3));
    asm volatile ("mov %0, %%cr3" :: "r" (virtual_to_physical((uint64_t)globalPTM.PML4)));

    cpu_local_data* local = get_cpu_local_data();
    local->lapic->tick_count++;

    if (local->lapic->id == 0 && local->lapic->tick_count % 1000 == 0) {
        current_time++;
        TSC::update_prev_cnt();
    }
    EOI();

    task_scheduler::scheduler_tick(regs);
    asm volatile ("mov %0, %%cr3" :: "r" (old_cr3));
}

extern "C" void task_scheduler_swap_task_cpp(__registers_t* regs){
    uint64_t old_cr3 = 0;
    asm volatile ("mov %%cr3, %0" : "=r" (old_cr3));
    asm volatile ("mov %0, %%cr3" :: "r" (virtual_to_physical((uint64_t)globalPTM.PML4)));
    
    task_scheduler::scheduler_tick(regs, true);
    asm volatile ("mov %0, %%cr3" :: "r" (old_cr3));
}