/* Defines the interrupt service routines */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cpu.h>

#define IDT_TA_InterruptGate    0b10001110
#define IDT_TA_USER_InterruptGate    0b11101110
#define IDT_TA_CallGate         0b10001100
#define IDT_TA_TrapGate         0b10001111

#define PIT_INTERRUPT_VECTOR 0x21
#define HPET_INTERRUPT_VECTOR 0x22
#define APIC_TIMER_INTERRUPT_VECTOR 0x23

#define PS2_PRI_PORT_VECTOR         0x24
#define PS2_SEC_PORT_VECTOR         0x25
#define PCI_INT_VECTOR              0x26
#define COM_PORT_INT_VECTOR         0x27

#define SCHEDULER_SWAP_TASKS_VECTOR 0x28
#define HALT_EXEC_INTERRUPT_VECTOR  0x29
#define SPURIOUS_INTERRUPT_VECTOR   0x2A

#define FIRST_FREE_VECTOR 0x30
extern uint8_t FreeVector;

uint8_t idt_allocate_vector();

struct __attribute__((packed)) isr_exception_info_t {
    // General Purpose Registers (Pushed by us)
    // We push these manually in the assembly stub
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, cr3, rax;

    // Interrupt Information (Pushed by us)
    // We push the interrupt number so the C++ handler knows which ISR fired
    uint64_t interrupt_number;
    
    // Error Code (Pushed by CPU or Dummy 0 by us)
    uint64_t error_code;

    // Interrupt Frame (Pushed automatically by CPU)
    uint64_t rip, cs, rflags, rsp, ss;
};

extern "C" void spurious_interrupt_handler();
extern "C" void spurious_interrupt_handler_cpp(void*);
extern "C" void apic_timer_handler();
extern "C" void apic_timer_handler_cpp(__registers_t* regs);
extern "C" void task_scheduler_swap_task();
extern "C" void task_scheduler_swap_task_cpp(__registers_t* regs);

// We don't use the cpu local struct so its safe to use __attribute__((interrupt))
__attribute__((interrupt)) void coprocessor_halt_execution(void*);



// stubs

extern "C" void isr0();
extern "C" void isr1();
extern "C" void isr2();
extern "C" void isr3();
extern "C" void isr4();
extern "C" void isr5();
extern "C" void isr6();
extern "C" void isr7();
extern "C" void isr8();
extern "C" void isr9();
extern "C" void isr10();
extern "C" void isr11();
extern "C" void isr12();
extern "C" void isr13();
extern "C" void isr14();
extern "C" void isr16();
extern "C" void isr17();
extern "C" void isr18();
extern "C" void isr19();
extern "C" void isr20();

typedef void (*dynamic_isr_handler_t)(void*);

// Dynamic ISRs
struct dynamic_isr_t{
    int identifier;
    dynamic_isr_handler_t isr;
    void* context;
    uint8_t vector;
    dynamic_isr_t* next;
};


int add_dynamic_isr(uint8_t vector, dynamic_isr_handler_t handler, void* context);
bool _remove_dynamic_isr(int identifier);

void setup_ap_interrupts();
void setup_bsp_interrupts();