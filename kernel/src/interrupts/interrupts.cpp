#include "interrupts.h"

__attribute__((interrupt)) void PageFault_Handler(InterruptFrameWithError* frame){
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

    if (taskScheduler::currentTask != nullptr){
        strcat(debug, "\nCurrent Task: ");
        strcat(debug, taskScheduler::currentTask->task_name);
        strcat(debug, "\nTask state: ");
        strcat(debug, 
            taskScheduler::currentTask->state == taskScheduler::task_state_t::Blocked ? "Blocked" :
            taskScheduler::currentTask->state == taskScheduler::task_state_t::Paused ? "Paused" :
            taskScheduler::currentTask->state == taskScheduler::task_state_t::Running ? "Running" :
            "Stopped"
        );
    }
    panic("Page fault detected", debug);
}


__attribute__((interrupt)) void DoubleFault_Handler(interrupt_frame* frame){
    panic("Double fault detected", "");
}


__attribute__((interrupt)) void GPFault_Handler(InterruptFrameWithError* frame){
    char* debug = (char*)GlobalAllocator.RequestPage();
    memset(debug, 0, 0x1000);

    debug[0] = '\n';
    strcat(debug, "ERROR CODE: ");
    strcat(debug, toHString(frame->error_code));
    strcat(debug, "\nRETURN ADDRESS: ");
    strcat(debug, toHString((uint64_t)frame->rip));

    if (taskScheduler::currentTask != nullptr){
        strcat(debug, "\nCurrent Task: ");
        strcat(debug, taskScheduler::currentTask->task_name);
        strcat(debug, "\nTask state: ");
        strcat(debug, 
            taskScheduler::currentTask->state == taskScheduler::task_state_t::Blocked ? "Blocked" :
            taskScheduler::currentTask->state == taskScheduler::task_state_t::Paused ? "Paused" :
            taskScheduler::currentTask->state == taskScheduler::task_state_t::Running ? "Running" :
            "Stopped"
        );
    }
    
    debug[strlen(debug)] = '\0';
    
    panic("General Protection fault detected", debug);
}

__attribute__((interrupt)) void KeyboardInt_Handler(interrupt_frame* frame){
    EOI();
    PS2KB::HandleKBInt();
}

__attribute__((interrupt)) void DivisionError_Handler(interrupt_frame* frame){
    panic("Division error detected", "");
}

__attribute__((interrupt)) void BoundRangeExceed_Handler(interrupt_frame* frame){
    panic("Bound range exceeded", "");
}

__attribute__((interrupt)) void InvalidOpcode_Handler(interrupt_frame* frame){
    panic("Invalid opcode exception", "");
}

__attribute__((interrupt)) void DeviceNotAvailable_Handler(interrupt_frame* frame){
    panic("Device not available exception", "");
}

__attribute__((interrupt)) void InvalidTSS_Handler(interrupt_frame* frame){
    panic("Invalid TSS exception", "");
}

__attribute__((interrupt)) void SegmentNotPresent_Handler(interrupt_frame* frame){
    panic("Segment not present exception", "");
}

__attribute__((interrupt)) void StackSegFault_Handler(InterruptFrameWithError* frame){
    char* debug = (char*)GlobalAllocator.RequestPage();
    memset(debug, 0, 0x1000);

    debug[0] = '\n';
    strcat(debug, "ERROR CODE: ");
    strcat(debug, toHString(frame->error_code));
    strcat(debug, "\nRETURN ADDRESS: ");
    strcat(debug, toHString((uint64_t)frame->rip));

    if (taskScheduler::currentTask != nullptr){
        strcat(debug, "\nCurrent Task: ");
        strcat(debug, taskScheduler::currentTask->task_name);
        strcat(debug, "\nTask state: ");
        strcat(debug, 
            taskScheduler::currentTask->state == taskScheduler::task_state_t::Blocked ? "Blocked" :
            taskScheduler::currentTask->state == taskScheduler::task_state_t::Paused ? "Paused" :
            taskScheduler::currentTask->state == taskScheduler::task_state_t::Running ? "Running" :
            "Stopped"
        );
    }
    
    debug[strlen(debug)] = '\0';
    panic("Stack-Segment fault", debug);
}

__attribute__((interrupt)) void x87FloatingPointEx_Handler(interrupt_frame* frame){
    panic("x87 floating point exception", "");
}

__attribute__((interrupt)) void AlignementCheck_Handler(interrupt_frame* frame){
    panic("Alignment check exception", "");
}

__attribute__((interrupt)) void SIMDFloatPointEx_Handler(interrupt_frame* frame){
    panic("SIMD Floating-Point exception", "");
}

__attribute__((interrupt)) void VirtualizationEx_Handler(interrupt_frame* frame){
    panic("Virtualization Exception", "");
}

__attribute__((interrupt)) void CPEx_Handler(interrupt_frame* frame){
    panic("Control Protection Exception", "");
}

__attribute__((interrupt)) void HypervisorInjEx_Handler(interrupt_frame* frame){
    panic("Hypervisor Injection Exception", "");
}

__attribute__((interrupt)) void VMMCommunicationEx_Handler(interrupt_frame* frame){
    panic("VMM Communication Exception", "");
}

__attribute__((interrupt)) void SecurityEx_Handler(interrupt_frame* frame){
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
    TickAPIC();
    taskScheduler::SchedulerTick(rbp, rsp);
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
    kprintf("{");
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

extern "C" void wm_mid_cpp_1(){

    asm volatile ("mov %0, %%cr3" :: "r" (taskScheduler::WM_TASK->ptm->PML4));
}

extern "C" void wm_mid_cpp_2(){
    if (taskScheduler::currentTask->ptm != nullptr){
        asm volatile ("mov %0, %%cr3" :: "r" (taskScheduler::currentTask->ptm->PML4));
    }else{
        asm volatile ("mov %0, %%cr3" :: "r" (globalPTM.PML4));
    }
}