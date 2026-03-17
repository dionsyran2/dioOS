#pragma once
#include <stdint.h>
#include <stddef.h>

#define APIC_TIMER_DCV 0x3E0 // Divide Configuration Register 
#define APIC_TIMER_LVT 0x320 // LVT Timer Register
#define APIC_TIMER_ICR 0x380 // Initial Count Register
#define APIC_TIMER_CCR 0x390 // Current Count Register

namespace APIC_TIMER{
    bool initialize_local_apic_timer();
    void ApicSleep(uint64_t ms);
}
