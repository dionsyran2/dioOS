#pragma once
#include <stdint.h>
#include <stddef.h>

#define PIT_BASE_FREQUENCY 1193182
namespace PIT{
    void Sleep(uint64_t milliseconds);

    void SetDivisor(uint16_t divisor);
    uint64_t GetFrequency();
    void SetFrequency(uint64_t frequency);

    bool initialize();
    uint64_t calibrate_tsc();

    void Tick();
}