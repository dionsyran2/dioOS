#pragma once
#include <stdint.h>
#include <BasicRenderer.h>
#include <panic.h>
#include <scheduling/apic/apic.h>
#include <scheduling/hpet/hpet.h>
#include <drivers/USB/XHCI.h>
#include <IO.h>
#include <UserInput/keyboard.h>
#include <UserInput/mouse.h>
#include <scheduling/pit/pit.h>
#include <drivers/serial.h>
#include <scheduling/task/scheduler.h>


#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01
#define PIC_EOI 0x20


#define IPI_VECTOR_HALT_EXECUTION 0xF1

struct InterruptFrameWithError {
    uint64_t error_code; // Error code, if present (e.g., page fault)
    uint64_t rip;        // Instruction Pointer (RIP)
    uint64_t cs;         // Code Segment (CS) selector
    uint64_t rflags;     // Flags register (RFLAGS)
    uint64_t rsp;        // Stack Pointer (RSP), only pushed if privilege changes
    uint64_t ss;         // Stack Segment (SS), only pushed if privilege changes
};
struct interrupt_frame;

typedef struct
{
	uint32_t ds;
	uint32_t edi, esi, esp, ebx, edx, ecx, eax;
	uint32_t int_num, err_code;
	uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;
__attribute__((interrupt)) void PageFault_Handler(InterruptFrameWithError* frame);
__attribute__((interrupt)) void DoubleFault_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void GPFault_Handler(InterruptFrameWithError* frame);

__attribute__((interrupt)) void DivisionError_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void BoundRangeExceed_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void InvalidOpcode_Handler(InterruptFrameWithError* frame);
__attribute__((interrupt)) void DeviceNotAvailable_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void InvalidTSS_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void SegmentNotPresent_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void StackSegFault_Handler(InterruptFrameWithError* frame);
__attribute__((interrupt)) void x87FloatingPointEx_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void AlignementCheck_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void SIMDFloatPointEx_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void VirtualizationEx_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void CPEx_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void HypervisorInjEx_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void VMMCommunicationEx_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void SecurityEx_Handler(interrupt_frame* frame);



__attribute__((interrupt)) void KeyboardInt_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void MouseInt_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void PitInt_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void HPETInt_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void APICInt_handler(interrupt_frame* frame);
__attribute__((interrupt)) void APICInit_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void SpuriousInt_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void SVGAIIIRQ(interrupt_frame* frame);
__attribute__((interrupt)) void syscallInt_handler(interrupt_frame* frame);
__attribute__((interrupt)) void SB16Int_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void PCIInt_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void XHCIInt_Handler(interrupt_frame* frame);

__attribute__((interrupt)) void TESTINT_Handler(interrupt_frame* frame);
__attribute__((interrupt)) void HLT_IPI_Handler(interrupt_frame* frame);

extern "C" void APICTimerInt_Handler(uint64_t rsp, uint64_t rbp);
extern "C" void APICTimerInt_ASM_Handler();


void RemapPIC();
void PIC_EndMaster();
void PIC_EndSlave();


typedef void (*isr_with_context)(void*);
/* STUBS */
struct ISR_CONTEXT{
    isr_with_context isr;
    void* context;
    uint8_t vector;
};

extern "C" void isr_dispatch(uint64_t vector, interrupt_frame* frame);
extern "C" void isr_common_stub();

void addIsrWithContext(uint8_t vector, void* handler, void* context);
void removeISR(void* handler);