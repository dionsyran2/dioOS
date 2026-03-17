#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cpu.h>
#include <kimmintrin.h>


namespace TSC{
    void update_prev_cnt();
    void calibrate_tsc();
    void nanosleep(uint64_t ns);
    
    uint64_t get_ticks();
    uint64_t get_time_ns();
    uint64_t get_ticks_per_ns();
}