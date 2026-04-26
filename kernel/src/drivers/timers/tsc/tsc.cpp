/* Use the cpu's TSC counter to get nanosecond precission */
#include <drivers/timers/tsc/tsc.h>
#include <drivers/timers/common.h>
#include <kstdio.h>


namespace TSC{
    uint64_t tsc_ticks_per_ns = 0;
    uint64_t prev_tsc_count = 0;

    uint64_t boot_tsc_count = 0;

    void update_prev_cnt(){
        prev_tsc_count = _rdtsc();
    }

    uint64_t get_time_ns(){
        return (_rdtsc() - prev_tsc_count) / tsc_ticks_per_ns;
    }

    uint64_t get_uptime_ns() {
        return (_rdtsc() - boot_tsc_count) / tsc_ticks_per_ns;
    }

    void calibrate_tsc(){
        if (is_hpet_enabled) tsc_ticks_per_ns = HPET::calibrate_tsc();
        if (is_pit_enabled) tsc_ticks_per_ns = PIT::calibrate_tsc();
        
        boot_tsc_count = _rdtsc();
        //kprintf("TSC Calibrated: %d ticks per ns\n", tsc_ticks_per_ns);
    }

    void nanosleep(uint64_t ns){
        uint64_t end_tick = _rdtsc() + (ns * tsc_ticks_per_ns);
        while (end_tick > _rdtsc()) asm ("pause");
    }

    uint64_t get_ticks(){
        return _rdtsc();
    }

    uint64_t get_ticks_per_ns(){
        return tsc_ticks_per_ns;
    }
    
}
