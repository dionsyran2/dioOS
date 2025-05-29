// https://github.com/dreamportdev/Osdev-Notes/blob/master/02_Architecture/07_APIC.md
#pragma once
#include <stdint.h>
#include "madt.h"
#include "../../interrupts/interrupts.h"
#include "../../IO.h"
#include "../../kernel.h"
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
|-------------------|-------------------|-------------------|-------------------|
| Reserved (24 bits)|   Destination ID (8 bits) |
|-------------------|-------------------|-------------------|-------------------|

ICR_LOW (32 bits)
|-------------------|-------------------|-------------------|-------------------|
| Delivery Mode (3 bits) |  Destination Shorthand (2 bits) |  Trigger Mode (1 bit) | Level (1 bit) | ... |
|-------------------|-------------------|-------------------|-------------------|
*/
#define ICR_LOW 0x300
#define ICR_HIGH 0x310

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

extern uint8_t TIME_DAY;
extern uint8_t TIME_MONTH;
extern uint16_t TIME_YEAR;
extern uint8_t TIME_HOUR;
extern uint8_t TIME_MINUTES;
extern uint8_t TIME_SECONDS;

extern uint64_t LAPICAddress;
extern uint64_t APICticsSinceBoot;
extern RTC::rtc_time_t* c_time;