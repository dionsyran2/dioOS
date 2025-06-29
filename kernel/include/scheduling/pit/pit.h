#pragma once
#include <stdint.h>
#include "../../BasicRenderer.h"
namespace PIT{
    extern double TimeSinceBoot;
    const uint64_t baseFrequency = 1193182;

    void SleepD(double seconds);
    void Sleep(uint64_t milliseconds);

    void SetDivisor(uint16_t divisor);
    uint64_t GetFrequency();
    void SetFrequency(uint64_t frequency);
    void Tick();
}