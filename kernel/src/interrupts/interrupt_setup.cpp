#include <interrupts/interrupts.h>
#include <local.h>

void setup_exception_handlers(){
    _set_interrupt_service_routine((void*)isr0, 0, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr1, 1, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr2, 2, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr3, 3, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr4, 4, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr5, 5, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr6, 6, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr7, 7, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr8, 8, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr9, 9, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr10, 10, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr11, 11, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr12, 12, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr13, 13, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr14, 14, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr16, 16, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr17, 17, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr18, 18, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr19, 19, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)isr20, 20, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)coprocessor_halt_execution, HALT_EXEC_INTERRUPT_VECTOR, IDT_TA_InterruptGate, 0x08);
}

void setup_bsp_interrupts(){
    setup_exception_handlers();
    _set_interrupt_service_routine((void*)spurious_interrupt_handler, SPURIOUS_INTERRUPT_VECTOR, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)task_scheduler_swap_task, SCHEDULER_SWAP_TASKS_VECTOR, IDT_TA_InterruptGate, 0x08);
}

void setup_ap_interrupts(){
    setup_exception_handlers();
    _set_interrupt_service_routine((void*)spurious_interrupt_handler, SPURIOUS_INTERRUPT_VECTOR, IDT_TA_InterruptGate, 0x08);
    _set_interrupt_service_routine((void*)task_scheduler_swap_task, SCHEDULER_SWAP_TASKS_VECTOR, IDT_TA_InterruptGate, 0x08);
}