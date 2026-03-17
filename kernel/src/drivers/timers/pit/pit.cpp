#include <drivers/timers/pit/pit.h>
#include <interrupts/interrupts.h>
#include <scheduling/apic/ioapic.h>
#include <IO.h>
#include <kstdio.h> // For debug prints
#include <local.h>
#include <kimmintrin.h>


namespace PIT {
    volatile uint64_t pit_ticks = 0;
    
    // 1000 Hz = 1 tick per ms
    const uint64_t ticks_per_ms = 1; 

    uint16_t Divisor = 65535;

    void SetDivisor(uint16_t divisor){
        if (divisor < 100) divisor = 100;
        Divisor = divisor;

        // 2. CRITICAL FIX: Send Command Byte first!
        // 0x43 is the Command Register.
        // 0x36 = Channel 0, Access Lo/Hi, Mode 3 (Square Wave), Binary
        outb(0x43, 0x36);

        // Now send the data to Channel 0 (0x40)
        outb(0x40, (uint8_t)(divisor & 0xFF));
        io_wait();
        outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    }

    uint64_t GetFrequency(){
        return PIT_BASE_FREQUENCY / Divisor;
    }

    void SetFrequency(uint64_t frequency){
        SetDivisor(PIT_BASE_FREQUENCY / frequency);
    }

    void pit_interrupt_handler(void*){
        pit_ticks++;
    }

    void Sleep(uint64_t ms){
        cpu_local_data* local = get_cpu_local_data();

        // Calculate target based on current ticks
        uint64_t target = pit_ticks + ms;
        
        // Ensure interrupts are on, otherwise we hang forever
        asm volatile("sti");

        if (local && local->cpu_id == 0){
            while (pit_ticks < target) asm ("hlt");
        }else{
            while (pit_ticks < target) asm ("pause"); // Don't halt, it will cause everything to break during initialization :)
        }
    }

    uint64_t calibrate_tsc() {
        // Wait 50ms
        uint64_t ms_to_wait = 50;
        uint64_t start_ticks = pit_ticks;
        
        uint64_t start_tsc = _rdtsc();
        
        // Wait for the PIT to count up 50 times
        while ((pit_ticks - start_ticks) < ms_to_wait) {
            asm volatile("pause");
        }
        
        uint64_t end_tsc = _rdtsc();
        
        // Math: 
        // Diff = Total TSC cycles elapsed
        // Time = 50ms = 50,000,000 ns
        uint64_t diff = end_tsc - start_tsc;
        
        return diff / (ms_to_wait * 1000000);
    }

    bool initialize(){
        // 1. Set Hardware Frequency (1000 Hz)
        SetFrequency(1000);

        // 2. Register Interrupt
        _add_dynamic_isr(PIT_INTERRUPT_VECTOR, pit_interrupt_handler, nullptr);
        
        // 3. Route IRQ 0 (or its override) to the Vector
        set_apic_irq(0, PIT_INTERRUPT_VECTOR, false);
        
        return true;
    }
}