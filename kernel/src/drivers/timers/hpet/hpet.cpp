#include <drivers/timers/hpet/hpet.h>
#include <paging/PageTableManager.h>
#include <interrupts/interrupts.h>
#include <scheduling/apic/ioapic.h>
#include <kimmintrin.h>

#include <local.h>
#include <memory.h>
#include <kstdio.h>

namespace HPET{
    // HPET Variables
    hpet_t* hpet = nullptr;
    uint64_t hpet_frequency = 0; // f = 10^15 / period
    uint64_t hpet_ticks_per_ms = 0;
    uint64_t hpet_address = 0;
    uint64_t hpet_ticks = 0;

    void write_register(uint16_t reg, uint64_t value){
        *(uint64_t*)(hpet_address + reg) = value;
    }

    uint64_t read_register(uint16_t reg){
        return *(uint64_t*)(hpet_address + reg);
    }

    uint64_t get_hpet_frequency(){
        uint64_t caps = read_register(HPET_CAPS_REG);
        uint32_t period_fs = (uint32_t)(caps >> 32);
        if (period_fs == 0) return 0;

        return FEMTOSECONDS_PER_SECOND / period_fs;
    }

    void hpet_interrupt_handler(void*);

    int configure_hpet_interrupt(int timer_index) {
        // Read the Configuration/Capabilities Register
        uint64_t reg_val = read_register(HPET_TIMER_CONF_CAP(timer_index));

        // This is a bitmap where bit n = 1 means irq n is supported.
        uint32_t allowed_irqs = (uint32_t)(reg_val >> HPET_TN_INT_ROUTE_CAP_SHIFT);

        // Find a valid IRQ line
        int chosen_irq = -1;
        
        if (read_register(HPET_CAPS_REG) & (1 << 15)) { // legacy replacement
            chosen_irq = -2;
        } else{
            for (int i = 0; i < 32; i++) {
                if (allowed_irqs & (1UL << i)) {
                    chosen_irq = i;

                    if (i > 15) break; // Stop at the first valid one
                }
            }

            if (chosen_irq == -1) {
                return -1; 
            }
        }

        if (chosen_irq >= 0){
            // Update the Register with the new configuration
            // Clear the old routing (Bits 9-13)
            reg_val &= ~HPET_TN_INT_ROUTE_CNF_MASK;
            
            // Set the new routing
            reg_val |= ((uint64_t)chosen_irq << HPET_TN_INT_ROUTE_CNF_SHIFT);
        }
        
        // Enable the Interrupt (Bit 2)
        reg_val |= HPET_TN_INT_ENABLE;

        write_register(HPET_TIMER_CONF_CAP(timer_index), reg_val);
        
        //kprintf("HPET Timer %d routed to IRQ %d\n", timer_index, chosen_irq);
        return chosen_irq;
    }

    bool initialize_hpet(){
        // Find the table
        hpet = (hpet_t*)ACPI::FindTable(xsdt, "HPET");
        if (!hpet) return false;
        
        // Map the address
        hpet_address = physical_to_virtual(hpet->address.Address);
        globalPTM.MapMemory((void*)hpet_address, (void*)hpet->address.Address);
        globalPTM.SetFlag((void*)hpet_address, PT_Flag::CacheDisable, true);

        // Reset the HPET
        uint64_t general_config = read_register(HPET_GENERAL_CONF_REG);
        general_config &= ~1UL;

        write_register(HPET_GENERAL_CONF_REG, general_config);
        while((general_config = read_register(HPET_GENERAL_CONF_REG)) & 1) asm ("pause");

        // Get the frequency
        hpet_frequency = get_hpet_frequency();
        hpet_ticks_per_ms = hpet_frequency / 1000;
        
        // Set up interrupts
        int irq = configure_hpet_interrupt(0);
        if (irq == -1) return false;
        
        _add_dynamic_isr(HPET_INTERRUPT_VECTOR, hpet_interrupt_handler, nullptr);
        set_apic_irq(irq == -2 ? 2 : irq, HPET_INTERRUPT_VECTOR, false, false, false, irq >= 0 ? true : false);

        // Configure the timer
        // Reset the counter to 0
        write_register(HPET_MAIN_CNT_VAL_REG, 0);

        // Set Configuration bits FIRST
        // Bit 2: Interrupt Enable
        // Bit 3: Periodic Mode
        // Bit 6: ValSet (Allows us to set the Period in step C)
        uint64_t config_reg = read_register(HPET_TIMER_CONF_CAP(0));
        config_reg |= (1 << 2) | (1 << 3) | (1 << 6);

        write_register(HPET_TIMER_CONF_CAP(0), config_reg);
        write_register(HPET_TIMER_COMP_VAL(0), hpet_ticks_per_ms);
        // Enable the HPET
        general_config = read_register(HPET_GENERAL_CONF_REG);
        general_config |= (irq == -2 ? 3 : 1);
        write_register(HPET_GENERAL_CONF_REG, general_config);

        while(((general_config = read_register(HPET_GENERAL_CONF_REG)) & 1) == 0) asm ("pause");

        hpet_ticks = 0;
        return true;
    }

    uint64_t calibrate_tsc() {
        // 1. Prepare for measurement
        // We wait for 10ms (10,000,000,000 femtoseconds)
        uint64_t target_fs = 10000000000000ULL; 
        uint64_t hpet_period = read_register(HPET_CAPS_REG) >> 32;
        
        // Calculate how many HPET ticks equal 10ms
        uint64_t target_hpet_ticks = target_fs / hpet_period;

        // 2. Start the clock
        uint64_t hpet_start = read_register(HPET_MAIN_CNT_VAL_REG);
        uint64_t tsc_start = _rdtsc();

        // 3. Wait for HPET to advance by 10ms
        // We use busy waiting here because we need precision, not interrupts.
        while (read_register(HPET_MAIN_CNT_VAL_REG) - hpet_start < target_hpet_ticks) {
            asm volatile("pause");
        }

        // 4. Stop the clock
        uint64_t tsc_end = _rdtsc();

        // 5. Calculate TSC frequency
        // We waited 10,000,000 nanoseconds
        uint64_t tsc_diff = tsc_end - tsc_start;
        uint64_t tsc_ticks_per_ns = tsc_diff / 10000000;

        return tsc_ticks_per_ns;
    }

    void hpet_interrupt_handler(void*){
        hpet_ticks++;
    }

    void Sleep(uint64_t ms){
        cpu_local_data* local = get_cpu_local_data();
        uint64_t final_tick = hpet_ticks + ms;

        bool intr = are_interrupts_enabled();
        asm ("sti");

        if (local && local->cpu_id == 0){
            while (final_tick > hpet_ticks) asm ("hlt");
        }else{
            while (final_tick > hpet_ticks) asm ("pause"); // Don't halt, it will cause everything to break during initialization :)
        }

        if (!intr) asm ("cli");
    }
}