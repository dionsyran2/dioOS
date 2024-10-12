#include "interrupts.h"
#include <cstdint>
#include "../IO.h"
#include "../UserInput/keyboard.h"
#include "../UserInput/mouse.h"
#include "../scheduling/pit/pit.h"
__attribute__((interrupt)) void PageFault_Handler( interrupt_frame* frame){
    panic("Page fault detected");
}


__attribute__((interrupt)) void DoubleFault_Handler(interrupt_frame* frame){
    panic("Double fault detected");
}


__attribute__((interrupt)) void GPFault_Handler( interrupt_frame* frame){
    panic("General Protection fault detected");
}

__attribute__((interrupt)) void KeyboardInt_Handler(interrupt_frame* frame){
    uint8_t scanCode = inb(0x60);
    HandleKeyboard(scanCode);
    PIC_EndMaster();

}

__attribute__((interrupt)) void MouseInt_Handler(interrupt_frame* frame){
    uint8_t mouseData = inb(0x60);
    HandlePS2Mouse(mouseData);
    PIC_EndSlave();
}

__attribute__((interrupt)) void PitInt_Handler(interrupt_frame* frame){
    PIT::Tick();
    PIC_EndMaster();
}

__attribute__((interrupt)) void APICInt_handler(interrupt_frame* frame) {
    globalRenderer->print("Received INTERRUPT!! \n");
    SendEndOfInterrupt();
}


extern "C" void __attribute__((section(".text.interrupt_handler")))__attribute__((interrupt)) APICInit_Handler(interrupt_frame* frame) {
    globalRenderer->print("APIC Init Handler triggered!\n");
    SecondaryCPUStart();
    SendEndOfInterrupt();
}
void RemapPIC(){
    uint8_t a1, a2; 

    a1 = inb(PIC1_DATA);
    io_wait();
    a2 = inb(PIC2_DATA);
    io_wait();

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    outb(PIC1_DATA, 4);
    io_wait();
    outb(PIC2_DATA, 2);
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, a1);
    io_wait();
    outb(PIC2_DATA, a2);

}


void PIC_EndMaster(){
    outb(PIC1_COMMAND, PIC_EOI);
}


void PIC_EndSlave(){
    outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}