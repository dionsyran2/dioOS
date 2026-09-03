#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cpu.h>
#include <scheduling/apic/common.h>
#include <interrupts/idt.h>
#include <gdt/gdt.h>
#include <kstdio.h>

#include <scheduling/task_scheduler/task_scheduler.h>
#include <scheduling/task_scheduler/cpu_task_queue.h>

struct task_t;
struct local_apic_t;


struct __attribute__((packed)) cpu_local_data {
    cpu_local_data* self_reference; // So we can do GS:0 to retrieve the address

    uint64_t scratch;
    uint64_t userspace_return_address;
    
    uint64_t cpu_id;
    local_apic_t* lapic;
    bool apic_timer_initialized;
    
    interrupt_descriptor_table_t* interrupt_descriptor_table;

    gdt_t* global_descriptor_table;
    tss_t* tss;
    
    task_t* current_task;
    task_t* idle_task;
    bool disable_scheduling;
    
    uint64_t time_in_userspace;
    uint64_t time_in_kernel;
    uint64_t update_tick_count;

    uint64_t kernel_stack_top;
    uint64_t user_stack_scratch;

    // Chain
    cpu_local_data *next;

    uint64_t idle_time;

    uint64_t scheduler_stack;

    cpu_task_queue_t *scheduler_queue;
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

inline void _set_interrupt_service_routine_for_all(void *ISR, uint8_t vector, uint8_t type_attr, uint8_t selector){
    uint64_t rflags = spin_lock(&_cpu_local_data_chain_lock);

    cpu_local_data *current_cpu = bsp_local;

    while (current_cpu){
        current_cpu->interrupt_descriptor_table->set_interrupt_handler(ISR, vector, type_attr, selector);
        current_cpu = current_cpu->next;
    }

    spin_unlock(&_cpu_local_data_chain_lock, rflags);
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