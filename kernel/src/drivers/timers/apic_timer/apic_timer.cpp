#include <drivers/timers/apic_timer/apic_timer.h>
#include <scheduling/apic/common.h>
#include <interrupts/interrupts.h>
#include <drivers/timers/common.h>
#include <local.h>

namespace APIC_TIMER{
    bool initialize_local_apic_timer(){
        const uint32_t CALIBRATION_MS = 10;

        cpu_local_data* local = get_cpu_local_data();

        // Setup the interrupt handler
        _set_interrupt_service_routine((void*)apic_timer_handler, APIC_TIMER_INTERRUPT_VECTOR, IDT_TA_InterruptGate, 0x08);
        // Set the divider
        local->lapic->write_register(APIC_TIMER_DCV, 0x3);

        // Set the LVT
        local->lapic->write_register(APIC_TIMER_LVT, APIC_TIMER_INTERRUPT_VECTOR | (1 << 17) | (0 << 16));

        // Set the initial count
        local->lapic->write_register(APIC_TIMER_ICR, 0xFFFFFFFF);
        Sleep(CALIBRATION_MS);
        uint32_t elapsed_ticks = 0xFFFFFFFF - local->lapic->read_register(APIC_TIMER_CCR); 

        local->lapic->tick_count = 0;
        uint64_t counts_per_ms = elapsed_ticks / CALIBRATION_MS;

        local->lapic->write_register(APIC_TIMER_LVT, 0x23 | (1 << 17) | (1 << 16)); // mask
        local->lapic->write_register(APIC_TIMER_ICR, counts_per_ms);   // Set the initial tick so it ticks every 1ms
        local->lapic->write_register(APIC_TIMER_LVT, 0x23 | (1 << 17) | (0 << 16)); // unmask

        if (local->lapic->id == 0){
            kprintf("\e[0;36m[APIC]\e[0m Counts per ms: %d\n", counts_per_ms);
        }

        local->lapic->ticks_per_ms = 1;

        return true;
    }

    void ApicSleep(uint64_t ms){
        if (!local_apic_list) return;

        uint64_t end_ticks = local_apic_list->tick_count + ms;
        
        while(local_apic_list->tick_count < end_ticks) {
            asm ("sti");
            asm ("hlt");
        }
    }
}