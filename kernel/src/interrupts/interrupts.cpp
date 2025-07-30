#include <interrupts/interrupts.h>
#include "kernel.h"
#include <ArrayList.h>
//#define PANIC_ON_EXCEPTION

void HaltExecution(){
    return;
    asm ("cli");
    for (size_t i = 0; i < ApplicationProcessors->length(); i++){
        uint16_t apic = ApplicationProcessors->get(i).APICID;
        if (apic == get_apic_id()) continue;

        SendIPI(IPI_VECTOR_HALT_EXECUTION, apic);
    }
    if (get_apic_id() != 0){
        SendIPI(IPI_VECTOR_HALT_EXECUTION, 0);
    }
}

__attribute__((interrupt)) void HLT_IPI_Handler(interrupt_frame* frame){
    return;
    asm ("cli");
    while(1);
}


__attribute__((interrupt)) void PageFault_Handler(InterruptFrameWithError* frame){
    HaltExecution();
    uint64_t address;
    __asm__ volatile ("mov %%cr2, %0" : "=r" (address));
    uint64_t errorCode = frame->error_code;
    uint8_t p = errorCode & 0b00000001;
    uint8_t w = (errorCode >> 1) & 0b00000001;
    uint8_t u = (errorCode >> 2) & 0b00000001;
    uint8_t r = (errorCode >> 3) & 0b00000001;
    uint8_t i = (errorCode >> 4) & 0b00000001;
    uint8_t pk = (errorCode >> 5) & 0b00000001;
    uint8_t ss = (errorCode >> 6) & 0b00000001;
    uint8_t SGX = (errorCode >> 15) & 0b00000001;




    char* debug = (char*)GlobalAllocator.RequestPage(); //Lets pray the page fault was not caused by request page
    memset(debug, 0, 0x1000);
    debug[0] = '\n';
    strcat(debug, (char*)toHString(address));
    strcat(debug, "\n");
    strcat(debug, p == 1 ? "page-protection violation\n" : "non-present page\n");
    strcat(debug, w == 1 ? "write access\n" : "read access\n");
    strcat(debug, u == 1 ? "Fault caused while in CPL = 3\n" : "Fault was caused outside CPL = 3\n");
    strcat(debug, r == 1 ? "one or more page directory entries contain reserved bits which are set to 1\n" : "no reserved bits are set to 1\n");
    strcat(debug, i == 1 ? "instruction fetch (while No-Execute bit is supported and enabled.)\n" : "no instruction fetch issue\n");
    strcat(debug, pk == 1 ? "protection-key violation\n" : "no protection-key violation\n");
    strcat(debug, ss == 1 ? "shadow stack access\n" : "no shadow stack access\n");
    strcat(debug, SGX == 1 ? "SGX violation\n" : "no SGX violation\n");
    strcat(debug, "Return address: ");
    strcat(debug, toHString(frame->rip));

    asm ("cli");
    task_t* ctask = task_scheduler::get_current_task();

    #ifndef PANIC_ON_EXCEPTION
    if (ctask){
        serialf("%s \n\r", debug);
        ctask->exit(128 + SIGSEGV); // terminate the task
    }
    #endif


    if (ctask != nullptr){
        strcat(debug, "\nCurrent Task: ");
        strcat(debug, ctask->name);
        strcat(debug, "\nTask state: ");
        strcat(debug, 
            ctask->state == task_state_t::BLOCKED ? "Blocked" :
            ctask->state == task_state_t::PAUSED ? "Paused" :
            ctask->state == task_state_t::RUNNING ? "Running" :
            ctask->state == task_state_t::ZOMBIE ? "Zombie" :
            ctask->state == task_state_t::DISABLED ? "Disabled" :
            "Stopped"
        );
    }
    panic("Page fault detected", debug);
}


__attribute__((interrupt)) void DoubleFault_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("Double fault detected", "");
}

__attribute__((interrupt)) void GPFault_Handler(InterruptFrameWithError* frame){
    uint64_t rcx = 0;
    asm ("mov %%rcx, %0" : "=r" (rcx));
    serialf("rcx: %llx/n/r\n", rcx);


    HaltExecution();
    char* debug = (char*)GlobalAllocator.RequestPage();
    memset(debug, 0, 0x1000);

    debug[0] = '\n';
    strcat(debug, "ERROR CODE: ");
    strcat(debug, toHString(frame->error_code));
    strcat(debug, "\nRETURN ADDRESS: ");
    strcat(debug, toHString((uint64_t)frame->rip));
    
    asm ("cli");
    task_t* ctask = task_scheduler::get_current_task();

    #ifndef PANIC_ON_EXCEPTION
    if (ctask){
        serialf("%s \n\r", debug);
        ctask->exit(128 + SIGSEGV); // terminate the task
    }
    #endif

    if (ctask != nullptr){
        strcat(debug, "\nCurrent Task: ");
        strcat(debug, ctask->name);
        strcat(debug, "\nTask state: ");
        strcat(debug, 
            ctask->state == task_state_t::BLOCKED ? "Blocked" :
            ctask->state == task_state_t::PAUSED ? "Paused" :
            ctask->state == task_state_t::RUNNING ? "Running" :
            ctask->state == task_state_t::ZOMBIE ? "Zombie" :
            ctask->state == task_state_t::DISABLED ? "Disabled" :
            "Stopped"
        );
    }
    
    
    debug[strlen(debug)] = '\0';
    
    panic("General Protection fault detected", debug);
}

__attribute__((interrupt)) void KeyboardInt_Handler(interrupt_frame* frame){
    bool prev = task_scheduler::disable_scheduling;
    task_scheduler::disable_scheduling = true;
    EOI();
    PS2KB::HandleKBInt();
    task_scheduler::disable_scheduling = prev;
}

__attribute__((interrupt)) void DivisionError_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("Division error detected", "");
}

__attribute__((interrupt)) void BoundRangeExceed_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("Bound range exceeded", "");
}

__attribute__((interrupt)) void InvalidOpcode_Handler(InterruptFrameWithError* frame){
    HaltExecution();
    char* debug = (char*)GlobalAllocator.RequestPage();
    memset(debug, 0, 0x1000);

    debug[0] = '\n';
    strcat(debug, "\nFAULT ADDRESS: ");
    strcat(debug, toHString((uint64_t)frame->error_code));

    task_t* ctask = task_scheduler::get_current_task();
    if (ctask != nullptr){
        strcat(debug, "\nCurrent Task: ");
        strcat(debug, ctask->name);
        strcat(debug, "\nTask state: ");
        strcat(debug, 
            ctask->state == task_state_t::BLOCKED ? "Blocked" :
            ctask->state == task_state_t::PAUSED ? "Paused" :
            ctask->state == task_state_t::RUNNING ? "Running" :
            ctask->state == task_state_t::ZOMBIE ? "Zombie" :
            ctask->state == task_state_t::DISABLED ? "Disabled" :
            "Stopped"
        );
    }
    panic("Invalid opcode exception", debug);
}

__attribute__((interrupt)) void DeviceNotAvailable_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("Device not available exception", "");
}

__attribute__((interrupt)) void InvalidTSS_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("Invalid TSS exception", "");
}

__attribute__((interrupt)) void SegmentNotPresent_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("Segment not present exception", "");
}

__attribute__((interrupt)) void StackSegFault_Handler(InterruptFrameWithError* frame){
    HaltExecution();
    char* debug = (char*)GlobalAllocator.RequestPage();
    memset(debug, 0, 0x1000);

    debug[0] = '\n';
    strcat(debug, "ERROR CODE: ");
    strcat(debug, toHString(frame->error_code));
    strcat(debug, "\nRETURN ADDRESS: ");
    strcat(debug, toHString((uint64_t)frame->rip));

    task_t* ctask = task_scheduler::get_current_task();
    if (ctask != nullptr){
        strcat(debug, "\nCurrent Task: ");
        strcat(debug, ctask->name);
        strcat(debug, "\nTask state: ");
        strcat(debug, 
            ctask->state == task_state_t::BLOCKED ? "Blocked" :
            ctask->state == task_state_t::PAUSED ? "Paused" :
            ctask->state == task_state_t::RUNNING ? "Running" :
            ctask->state == task_state_t::ZOMBIE ? "Zombie" :
            ctask->state == task_state_t::DISABLED ? "Disabled" :
            "Stopped"
        );
    }
    
    debug[strlen(debug)] = '\0';
    panic("Stack-Segment fault", debug);
}

__attribute__((interrupt)) void x87FloatingPointEx_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("x87 floating point exception", "");
}

__attribute__((interrupt)) void AlignementCheck_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("Alignment check exception", "");
}

__attribute__((interrupt)) void SIMDFloatPointEx_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("SIMD Floating-Point exception", "");
}

__attribute__((interrupt)) void VirtualizationEx_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("Virtualization Exception", "");
}

__attribute__((interrupt)) void CPEx_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("Control Protection Exception", "");
}

__attribute__((interrupt)) void HypervisorInjEx_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("Hypervisor Injection Exception", "");
}

__attribute__((interrupt)) void VMMCommunicationEx_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("VMM Communication Exception", "");
}

__attribute__((interrupt)) void SecurityEx_Handler(interrupt_frame* frame){
    HaltExecution();
    panic("Security Exception", "");
}

__attribute__((interrupt)) void MouseInt_Handler(interrupt_frame* frame){
    PS2Mouse::HandlePS2Mouse();
    EOI();
}

__attribute__((interrupt)) void PitInt_Handler(interrupt_frame* frame){
    kprintf("-");
    PIC_EndMaster();
}

__attribute__((interrupt)) void HPETInt_Handler(interrupt_frame* frame){
    HPET::tick();
    EOI();
}

extern "C" void APICTimerInt_Handler(uint64_t rsp, uint64_t rbp){
    EOI();
    
    if (get_apic_id() == 0) TickAPIC();

    task_scheduler::tick(rbp, rsp);
}

__attribute__((interrupt)) void SpuriousInt_Handler(interrupt_frame* frame){
    EOI();
}

__attribute__((interrupt)) void syscallInt_handler(interrupt_frame* frame){
    //syscall_handler();
}

__attribute__((interrupt)) void SB16Int_Handler(interrupt_frame* frame){
    EOI();
    //HDA::RefillBuffer();
}

__attribute__((interrupt)) void PCIInt_Handler(interrupt_frame* frame){
    //kprintf("{");
    EOI();
    //hdaInstances[0]->RefillBuffer();
}

__attribute__((interrupt)) void XHCIInt_Handler(interrupt_frame* frame){
    //XHCI::GenIntHndlr();
    EOI();
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
    outb(PIC2_DATA, a2);

}


void PIC_EndMaster(){
    outb(PIC1_COMMAND, PIC_EOI);
}


void PIC_EndSlave(){
    outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

/*STUBS*/
ArrayList<ISR_CONTEXT*>* isrs = nullptr;
extern "C" void* isr_stub_table_ptrs[256];

void addIsrWithContext(uint8_t vector, void* handler, void* context){
    if (isrs == nullptr) isrs = new ArrayList<ISR_CONTEXT*>();
    ISR_CONTEXT* isr = new ISR_CONTEXT;
    isr->context = context;
    isr->vector = vector;
    isr->isr = (isr_with_context)handler;

    isrs->add(isr);

    SetIDTGate(isr_stub_table_ptrs[vector], vector, IDT_TA_InterruptGate, 0x08, &idtr);
}

void removeISR(void* handler){
    for (int i = 0; i < isrs->length(); i++){
        ISR_CONTEXT* isr = isrs->get(i); 
        if (isr->isr == (isr_with_context)handler){
            isrs->remove(isr);
            delete isr;
            
            break;
        }
    }
}

extern "C" void isr_dispatch(uint64_t vector, interrupt_frame* frame){
    //kprintf("Vector: %hh\n", vector);
    for (int i = 0; i < isrs->length(); i++){
        ISR_CONTEXT* isr = isrs->get(i); 
        if (isr->vector == vector){
            //kprintf("Firing handler for vector %hh\n", vector);

            isr->isr(isr->context);
        }
    }

    EOI();
}

__attribute__((interrupt)) void TESTINT_Handler(interrupt_frame* frame){
    kprintf("IPI!!! %d\n", get_apic_id());
    EOI();
}