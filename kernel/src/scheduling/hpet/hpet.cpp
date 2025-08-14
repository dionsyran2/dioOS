#include <scheduling/hpet/hpet.h>
namespace HPET
{
    hpet* hdr;
    uint64_t* GCReg;
    volatile uint8_t *hpet_base;
    uint64_t ticks = 0;
    uint64_t ticksIn1MS = 0;
    uint64_t* counter;
    void Initialize(){
        void* hpet_hdr = ACPI::FindTable(xsdt, "HPET");
        hdr = (hpet*)hpet_hdr;

        uint64_t physical_address = hdr->address.address;
        uint64_t virtual_address = physical_to_virtual(physical_address);
        globalPTM.MapMemory((void*)virtual_address, (void*)physical_address);
        hpet_base = (uint8_t*) virtual_address;

        GCReg = (uint64_t*)(hpet_base + HPET_GLOBAL_CONF_REG);
        *GCReg &= ~1;
        while ((*GCReg & 1));


        //2. Calculate HPET frequency (f = 10^15 / period).
        uint32_t COUNTER_CLK_PERIOD = (uint32_t)((*(uint64_t*)hpet_base) >> 32);
        uint64_t f = (1000000000000000) / COUNTER_CLK_PERIOD;
        ticksIn1MS = f / 1000;
        //kprintf("TICKS: %d / %d\n", ticksIn1MS, hdr->minimum_tick);

        uint32_t irqs = (*(uint64_t*)(hpet_base + HPET_TIMER_CONF_CAP(0))) >> 32;
        *(uint32_t*)(hpet_base + HPET_TIMER_CONF_CAP(0)) &= ~0xFFFF;
        /*for (int i = 23; i >= 0; i--){
            if ((irqs >> i) & 1){
                SetIrq(i, 0x22, 0);
                *(uint32_t*)(hpet_base + HPET_TIMER_CONF_CAP(0)) &= ~(0b111111 | (0b11111 << 9));
                *(uint32_t*)(hpet_base + HPET_TIMER_CONF_CAP(0)) |= (i << 9);
                kprintf("[HPET] Setting up irq %d\n", i);
                for (int i = 0; i < 1000000; i++);
                if ((((*(uint32_t*)(hpet_base + HPET_TIMER_CONF_CAP(0))) >> 9) & 0b11111) != i){
                    kprintf("Reading: %d\n", ((*(uint32_t*)(hpet_base + HPET_TIMER_CONF_CAP(0))) >> 9) & 0b11111);
                    SetIrq(i, 0, 1);
                    continue;
                }
                break;
            }
        }*/
        SetIrq(2, 0x22, 0);
        uint32_t* conf = (uint32_t*)(hpet_base + HPET_TIMER_CONF_CAP(0));
        *conf |= (1 << 2) | (1 << 3) | (1 << 6);
        counter = (uint64_t*)(hpet_base + HPET_MAIN_CNT_VAL_REG);
        *counter = 0;
        *(uint64_t*)(hpet_base + HPET_TIMER_COMP_VAL(0)) = ticksIn1MS; // Set the comp value.
        *GCReg &= ~0b11;
        while ((*GCReg & 0b11));
        *GCReg |= 0b11;
        while (!(*GCReg & 0b11));

        kprintf(0x00FF00, "[HPET] ");
        kprintf("Waiting for the first interrupt\n");
        while(ticks == 0){
            __asm__ volatile ("hlt");
        }
        kprintf(0x00FF00, "[HPET] ");
        kprintf("Interrupt received!\n");
        
    }
    
    void DisableHPET(){
        SetIrq(2, 0, 1);
        *GCReg &= ~0b11;
    }

    void tick(){
        ticks++;
    }

    void Sleep(uint64_t ms){
        uint64_t startTicks = ticks;
        uint64_t targetTicks = startTicks + ms;
        
        while(ticks < targetTicks){
            __asm__ volatile ("nop");
        }
    }
}
