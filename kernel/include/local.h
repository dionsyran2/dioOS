#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cpu.h>
#include <scheduling/apic/common.h>
#include <interrupts/idt.h>
#include <gdt/gdt.h>
#include <kstdio.h>

#include <scheduling/task_scheduler/task_scheduler.h>
struct task_t;
struct local_apic_t;


struct __attribute__((packed)) cpu_local_data {
    // 1. Self reference
    cpu_local_data* self_reference; // So we can do GS:0 to retrieve the address

    uint64_t scratch;
    uint64_t userspace_return_address;
    
    // 2. Processor Identification
    uint64_t cpu_id;
    local_apic_t* lapic;
    bool apic_timer_initialized;
    
    // 3. Interrupts
    interrupt_descriptor_table_t* interrupt_descriptor_table;

    // 4. GDT
    gdt_t* global_descriptor_table;
    tss_t* tss;
    
    // 5. Scheduling Info
    task_t* current_task;
    task_t* idle_task;
    bool disable_scheduling;
    
    /* Utilization */
    uint64_t time_in_userspace;
    uint64_t time_in_kernel;
    uint64_t update_tick_count;

    // 6. Syscall Scratch Space
    uint64_t user_stack_scratch;
    uint64_t kernel_stack_top;

    // Chain
    cpu_local_data *next;

    // I totally didn't forget this
    uint64_t idle_time;

    uint64_t scheduler_stack;
};

extern spinlock_t _cpu_local_data_chain_lock;
extern cpu_local_data* _last_local_data_entry;
extern cpu_local_data* bsp_local;


// @brief retrieves the cpu local data struct
inline cpu_local_data* get_cpu_local_data(){
    cpu_local_data* result;
    asm volatile("movq %%gs:0, %0" : "=r"(result)); // Reads the self reference

    if (result == nullptr || result->self_reference != result) return nullptr;
    return result;
}

// @brief Sets an ISR for the local processor
inline void _set_interrupt_service_routine(void* ISR, uint8_t vector, uint8_t type_attr, uint8_t selector){
    cpu_local_data* local = get_cpu_local_data();

    if (!local || local != local->self_reference) return kprintf("\e[0;31m[SET_ISR]\e[0m The Core Local Data structure is pointing to an invalid memory region!\n");

    local->interrupt_descriptor_table->set_interrupt_handler(ISR, vector, type_attr, selector);
}

// @brief Sets an ISR for the local processor
inline void _set_bsp_interrupt_service_routine(void* ISR, uint8_t vector, uint8_t type_attr, uint8_t selector){
    cpu_local_data* local = bsp_local;

    if (!local || local != local->self_reference) return kprintf("\e[0;31m[SET_ISR]\e[0m The Core Local Data structure is pointing to an invalid memory region!\n");

    local->interrupt_descriptor_table->set_interrupt_handler(ISR, vector, type_attr, selector);
}
// @brief Signals EOI (End Of Interrupt) to the local apic
inline void EOI(){
    cpu_local_data* local = get_cpu_local_data();

    if (!local || local != local->self_reference) return kprintf("\e[0;31m[EOI]\e[0m The Core Local Data structure is pointing to an invalid memory region!\n");

    local->lapic->EOI();
}

inline void set_tss_rsp0(uint64_t rsp){
    cpu_local_data* local = get_cpu_local_data();

    if (!local || local != local->self_reference) return kprintf("\e[0;31m[set_tss_rsp0]\e[0m The Core Local Data structure is pointing to an invalid memory region!\n");

    local->tss->rsp0 = rsp;
}