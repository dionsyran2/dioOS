#pragma once
#include "../BasicRenderer.h"
#include "../panic.h"
#include "../scheduling/apic/apic.h"
#include "../kernelUtil.h"
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01
#define PIC_EOI 0x20


struct interrupt_frame;
__attribute__((interrupt)) void PageFault_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void DoubleFault_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void GPFault_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void KeyboardInt_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void MouseInt_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void PitInt_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void APICInt_handler(interrupt_frame* frame);
extern "C" void __attribute__((section(".text.interrupt_handler")))__attribute__((interrupt)) APICInit_Handler(interrupt_frame* frame);




void RemapPIC();
void PIC_EndMaster();
void PIC_EndSlave();