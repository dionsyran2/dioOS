#include <drivers/timers/common.h>
#include <drivers/timers/rtc/rtc.h>
#include <local.h>
#include <kstdio.h>

bool is_hpet_enabled = false;
bool is_pit_enabled = false;
uint64_t current_time;
uint64_t boot_time;

void initialize_timers(){
    cpu_local_data* local = get_cpu_local_data();

    is_hpet_enabled = false;
    is_pit_enabled = false;
    
    current_time = RTC::read_rtc_time();
    boot_time = current_time;
    
    is_hpet_enabled = HPET::initialize_hpet();
    if (!is_hpet_enabled) {
        kprintf("\e[0;33m[Timers]\e[0m Failed to initialize the HPET\n");
        is_pit_enabled = PIT::initialize();
    }
    local->apic_timer_initialized = false;
    local->apic_timer_initialized = APIC_TIMER::initialize_local_apic_timer();
    
    TSC::calibrate_tsc();
}

void Sleep(uint64_t ms){
    cpu_local_data* local = get_cpu_local_data();
    
    if (local->apic_timer_initialized) return APIC_TIMER::ApicSleep(ms);
    if (is_hpet_enabled) return HPET::Sleep(ms);
    if (is_pit_enabled) return PIT::Sleep(ms);
}

void nanosleep(uint64_t ns){
    return TSC::nanosleep(ns);
}

uint64_t GetTicks(){
    local_apic_t* lapic = local_apic_list;
    if (!lapic) return 0;
    return lapic->tick_count;
}

uint64_t GetTicksPerMS(){
    local_apic_t* lapic = local_apic_list;
    if (!lapic) return 1;
    return lapic->ticks_per_ms;
}