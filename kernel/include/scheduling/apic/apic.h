// https://github.com/dreamportdev/Osdev-Notes/blob/master/02_Architecture/07_APIC.md
#pragma once
#include <stdint.h>
#include "madt.h"
#include "../../interrupts/interrupts.h"
#include "../../IO.h"
#include "../../drivers/serial.h"
#include "../../IO.h"
#include "../pit/pit.h"
#include "ap_init.h"
#include "../../drivers/rtc/rtc.h"
#include "../../UserInput/inputClientEvents.h"

// PS/2 Devices
#define KEYBOARD_INTERRUPT 0x21
#define MOUSE_INTERRUPT    0x2C
//APIC
#define IA32_APIC_BASE 0x1B
#define SPURIOUS_VECTOR 0xF0

// I/O APIC
#define IRQ_VECTOR_BASE 0x20
#define IOREGSEL 0x00
#define IOWIN 0x10
#define IOAPIC_ID 0x00
#define IOAPIC_REDIRECTION_BASE 0x10







//IPI
/*
ICR_HIGH (32 bits)
|-------------------|---------------------------|
| Reserved (24 bits)|   Destination ID (8 bits) |
|-------------------|---------------------------|

ICR_LOW (32 bits)
|------------------------|---------------------------------|-------------------|-------------------|
| Delivery Mode (3 bits) |  Destination Shorthand (2 bits) |  Trigger Mode (1 bit) | Level (1 bit) | ... |
|------------------------|---------------------------------|-------------------|-------------------|
*/
#define ICR_LOW 0x300
#define ICR_HIGH 0x310

#define IPI_DELIVERY_MODE_NORMAL                (0 << 8)
#define IPI_DELIVERY_MODE_LOWEST_PRIORITY       (1 << 8)
#define IPI_DELIVERY_MODE_SMI                   (2 << 8)
#define IPI_DELIVERY_MODE_NMI                   (4 << 8)
#define IPI_DELIVERY_MODE_INIT                  (5 << 8) // Can either be INIT or INIT level de-asserte. INIT level de-asserte is not supported and ignored by modern cpus 
#define IPI_DELIVERY_MODE_SIPI                  (6 << 8)

#define IPI_DESTINATION_MODE_PHYSICAL           (0 << 11)
#define IPI_DESTINATION_MODE_LOGICAL            (1 << 11)

#define IPI_DELIVERY_STATUS                     (1 << 12)

/*
BIT 14: Clear for INIT LEVEL de-asserte, set otherwise
BIT 15: Set for INIT level de-asserte, clear otherwise 
*/
#define IPI_INIT_LEVEL_DE_ASSERTE               (1 << 15)
#define IPI_NOT_INIT_LEVEL_DE_ASSERTE           (1 << 14)


/*
BITS 18-19: Destination type. If this is > 0 then the destination field in 0x310 is ignored.
1 will always send the interrupt to itself, 2 will send it to all processors,and 3 will send
it to all processors aside from the current one. It is best to avoid using modes 1, 2 and 3, 
and stick with 0. 
*/
#define IPI_DESTINATION_TYPE_ITSELF             (1 << 18)
#define IPI_DESTINATION_TYPE_ALL                (2 << 18)
#define IPI_DESTINATION_TYPE_OTHERS             (3 << 18)




void InitLAPIC();
void InitIOAPIC();
void EOI();
void SendIPI(uint8_t vector, uint16_t APICID);
void SendInitIPI(uint16_t APICID);
void SendStartupIPI(uint16_t APICID);
void TickAPIC();
void Sleep(uint32_t ms);
uint64_t GetAPICTick();
void SetIrq(uint8_t irq, uint8_t vector, uint8_t mask);
void SetupAPICTimer();
int IRQtoGSI(int irq);
uint32_t get_apic_id();
uint64_t to_unix_timestamp(int sec, int min, int hour, int day, int month, int year);
uint64_t to_unix_timestamp(RTC::rtc_time_t* time);

extern uint64_t LAPICAddress;
extern uint64_t APICticsSinceBoot;
extern RTC::rtc_time_t* c_time;