#include <scheduling/pit/pit.h>
#include <IO.h>

namespace PIT{
    double TimeSinceBoot = 0;

    uint16_t Divisor = 65535;
    
    void SleepD(double seconds){
        double startTime = TimeSinceBoot;
        while (TimeSinceBoot < (startTime + seconds)){
            //asm("hlt");
        }
    }

    void Sleep(uint64_t milliSeconds){
        SleepD((double)milliSeconds /1000);
    }

    void SetDivisor(uint16_t divisor){
        if (divisor < 100) divisor = 100;
        Divisor = divisor;
        outb(0x40, (uint8_t)(divisor & 0x00ff));
        io_wait();
        outb(0x40, (uint8_t)((divisor & 0xff00) >> 8));
    }

    uint64_t GetFrequency(){
        return baseFrequency / Divisor;
    }

    void SetFrequency(uint64_t frequency){
        SetDivisor(baseFrequency / frequency);
    }

    void Tick(){
        TimeSinceBoot += (1 / (double)GetFrequency());
    }

}