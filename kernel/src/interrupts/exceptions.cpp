#include <interrupts/interrupts.h>
#include <local.h>
#include <drivers/serial/serial.h>
#include <paging/PageFrameAllocator.h>
#include <kstdio.h>
#include <panic.h>
#include <memory.h>
#include <scheduling/apic/lapic.h>

struct stack_frame_t {
    struct stack_frame_t* rbp;
    uint64_t rip;
};

struct MapRange {
    uint64_t start_virt;
    uint64_t end_virt;
    uint64_t flags; // We only group if flags are identical
    bool active;
};

// Helper to format flags nicely (e.g., "RW U" or "R- S")
void print_flags(uint64_t flags) {
    bool present = flags & (1 << 0);
    bool rw = flags & (1 << 1);
    bool user = flags & (1 << 2);
    bool nx = flags & (1UL << 63);

    serialf("%s%s%s%s", 
        present ? "P" : "-",
        rw ? "W" : "R",
        user ? "U" : "S",
        nx ? "N" : "X"
    );
}

void dump_stack_trace(stack_frame_t* stack) {
    kprintf("--- STACK TRACE ---\n");

    int depth = 0;
    while (stack != nullptr && depth < 20) {
        if ((uint64_t)stack < MEMORY_BASE) {
             break; 
        }

        uint64_t address = stack->rip;
        
        kprintf("[%d] 0x%p", depth, address);

        // Add kernel symbol table parsing
        kprintf("\n");
        
        stack = stack->rbp;
        depth++;
    }
    kprintf("--- END ---\n");
}

void log_registers(bool serial, isr_exception_info_t* registers){
    if (serial){
        serialf("\e[4;31m!----------------- REGISTER DUMP -----------------!\n\e[0m");
        serialf("RAX=%p\tRBX=%p\tRCX=%p \nRDX=%p\tRSI=%p\tRDI=%p \nRBP=%p\tRSP=%p\tR8=%p \nR9=%p \tR10=%p\tR11=%p \nR12=%p\tR13=%p\tR14=%p \nR15=%p\tRIP=%p\tCR3=%p\n",
            registers->rax, registers->rbx, registers->rcx, registers->rdx, registers->rsi,
            registers->rdi, registers->rbp, registers->rsp, registers->r8, registers->r9,
            registers->r10, registers->r11, registers->r12, registers->r13, registers->r14,
            registers->r15, registers->rip, registers->cr3
        );
    }else{
        kprintf("\e[4;31m!----------------- REGISTER DUMP -----------------!\n\e[0m");
        kprintf("RAX=%p\tRBX=%p\tRCX=%p \nRDX=%p\tRSI=%p\tRDI=%p \nRBP=%p\tRSP=%p\tR8=%p \nR9=%p \tR10=%p\tR11=%p \nR12=%p\tR13=%p\tR14=%p \nR15=%p\tRIP=%p\tCR3=%p\n\n",
            registers->rax, registers->rbx, registers->rcx, registers->rdx, registers->rsi,
            registers->rdi, registers->rbp, registers->rsp, registers->r8, registers->r9,
            registers->r10, registers->r11, registers->r12, registers->r13, registers->r14,
            registers->r15, registers->rip, registers->cr3
        );
    }
}

void PageFault(isr_exception_info_t* info){
    uint64_t address;
    __asm__ volatile ("mov %%cr2, %0" : "=r" (address));
    
    task_t* self = task_scheduler::get_current_task();

    if (self){
        // AAAAAAAAAAAAAHHH, NEVER MODIFY THE DAMN FPU... I HAVE BEEN CHASING THIS BUG FOR 2 DAYS
        save_fpu_state(self->saved_fpu_state);

        uint64_t pg = address & ~0xFFFUL;
        int flags = self->vm_tracker->get_flags(pg);
        if (flags & VM_PENDING_COW == 0) goto continue_pf_exception;

        uint64_t physical = self->ptm->getPhysicalAddress((void*)pg);
        if (!physical) goto continue_pf_exception;

        // Allocate a new page
        void* page = GlobalAllocator.RequestPage();
        if (!page) goto continue_pf_exception;

        // Copy the data
        memcpy(page, (void*)physical_to_virtual(physical), PAGE_SIZE);

        // Remove old reference
        GlobalAllocator.DecreaseReferenceCount((void*)physical);

        // Map the new page
        uint64_t new_physical = virtual_to_physical((uint64_t)page);
        self->ptm->MapMemory((void*)pg, (void*)new_physical);

        // Add flags
        if (flags & VM_FLAG_RW)
            self->ptm->SetFlag((void*)pg, PT_Flag::Write, true);

        if (flags & VM_FLAG_NX)
            self->ptm->SetFlag((void*)pg, PT_Flag::NX, true);
        
        if (flags & VM_FLAG_CD)
            self->ptm->SetFlag((void*)pg, PT_Flag::CacheDisable, true);
        
        if (flags & VM_FLAG_WT)
            self->ptm->SetFlag((void*)pg, PT_Flag::WriteThrough, true);
        
        self->ptm->SetFlag((void*)pg, PT_Flag::User, true);

        // Remove the pending flag
        self->vm_tracker->set_flags(pg, PAGE_SIZE, flags & ~VM_PENDING_COW);

        restore_fpu_state(self->saved_fpu_state);
        // Return execution
        return;
    }

continue_pf_exception:
    send_ipi(HALT_EXEC_INTERRUPT_VECTOR, 0, OTHERS);
    PageTableManager* ptm = &globalPTM;
    if (self && self->ptm) ptm = self->ptm;


    log_registers(false, info);
    dump_stack_trace((stack_frame_t*)info->rbp);
    uint64_t errorCode = info->error_code;
    uint8_t p = errorCode & 0b00000001;
    uint8_t w = (errorCode >> 1) & 0b00000001;
    uint8_t u = (errorCode >> 2) & 0b00000001;
    uint8_t r = (errorCode >> 3) & 0b00000001;
    uint8_t i = (errorCode >> 4) & 0b00000001;
    uint8_t pk = (errorCode >> 5) & 0b00000001;
    uint8_t ss = (errorCode >> 6) & 0b00000001;
    uint8_t SGX = (errorCode >> 15) & 0b00000001;

    panic("Page Fault!\nFault Address: %p\nFlags:\n\e[0;33m%s%s%s%s%s%s%s%s\e[0m\nPaging Raw Value: %p\nRunning task: %s (%d)\n",
        address,
        p ? "Page Protection Violation\n" : "Non-Present Page\n",
        w ? "Write Access\n" : "Read Access\n",
        u ? "CPL = 3\n" : "",
        r && p ? "Reserved bits are set!\n" : "",
        i && p ? "Instruction Fetch while NX is set\n" : "",
        pk  && p  ? "Protection-Key violation\n" : "",
        ss  && p  ? "Shadow Stack access\n" : "",
        SGX && p  ? "SGX violation\n" : "",
        ptm->getMapping((void*)address),
        self ? self->name : "None",
        self ? self->pid : 0
    );
}

void DoubleFault(isr_exception_info_t* info){
    send_ipi(HALT_EXEC_INTERRUPT_VECTOR, 0, OTHERS);
    log_registers(false, info);

    panic("Double fault!");
}

void GeneralProtection(isr_exception_info_t* info){
    send_ipi(HALT_EXEC_INTERRUPT_VECTOR, 0, OTHERS);
    log_registers(false, info);
    task_t *self = task_scheduler::get_current_task();

    stack_frame_t* stack = (stack_frame_t*)info->rbp;
    dump_stack_trace(stack);
    panic("General Protection Fault!\n Error Code: %p\n Running task: %s (%d)\n", info->error_code, self ? self->name : "None", self ? self->pid : 0);
}

void DivisionError(isr_exception_info_t* info){
    send_ipi(HALT_EXEC_INTERRUPT_VECTOR, 0, OTHERS);
    log_registers(false, info);

    stack_frame_t* stack = (stack_frame_t*)info->rbp;
    dump_stack_trace(stack);

    panic("Division Error!");
}

void InvalidOpcode(isr_exception_info_t* info){
    send_ipi(HALT_EXEC_INTERRUPT_VECTOR, 0, OTHERS);
    log_registers(false, info);

    stack_frame_t* stack = (stack_frame_t*)info->rbp;
    dump_stack_trace(stack);
    panic("Invalid Opcode Exception!\n Error Code: %p", info->error_code);
}

void Debug(isr_exception_info_t* info){
    send_ipi(HALT_EXEC_INTERRUPT_VECTOR, 0, OTHERS);
    log_registers(false, info);

    stack_frame_t* stack = (stack_frame_t*)info->rbp;
    dump_stack_trace(stack);

    uint64_t rflags = get_cpu_flags();
    rflags &= ~(1UL << 8); // Clear the trap flag
    set_cpu_flags(rflags);
    
    panic("Debug Exception (Should not be happening...)\n");
}
extern "C" void exception_handler(isr_exception_info_t* info){
    uint64_t old_cr3 = 0;
    asm volatile ("mov %%cr3, %0" : "=r" (old_cr3));
    asm volatile ("mov %0, %%cr3" :: "r" (global_ptm_cr3));

    /*stack_frame_t* stack = (stack_frame_t*)info->rbp;
    dump_stack_trace(stack);*/
    
    switch (info->interrupt_number){
        case 0x0:
            DivisionError(info);
            break;
        case 0x1:
            Debug(info);
            break;
        case 0x6:
            InvalidOpcode(info);
            break;
        case 0x8:
            DoubleFault(info);
            break;
        case 0xD:
            GeneralProtection(info);
            break;
        case 0xE: // PageFault
            PageFault(info);
            break;
        default:
            log_registers(false, info);
            panic("Unknown Exception: %.2x\n", info->interrupt_number);
            break;
    }

    asm volatile ("mov %0, %%cr3" :: "r" (old_cr3));
    return;
}
