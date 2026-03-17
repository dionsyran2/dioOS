#pragma once
#include <drivers/timers/pit/pit.h>
#include <drivers/timers/hpet/hpet.h>
#include <drivers/timers/tsc/tsc.h>
#include <drivers/timers/apic_timer/apic_timer.h>


void initialize_timers(); // HPET / PIT
void initialize_local_timer(); // APIC Timer

// @brief Spins for specified ms
void Sleep(uint64_t ms);

// @brief Spins for specified ns
void nanosleep(uint64_t ns);

// @brief Returns the current value of the tick count for the apic timer
uint64_t GetTicks();

// @brief Returns the ticks per second for the apic timer (usually 1)
uint64_t GetTicksPerMS();

extern bool is_hpet_enabled;
extern bool is_pit_enabled;


extern uint64_t current_time; // UNIX time
extern uint64_t boot_time; // UNIX time