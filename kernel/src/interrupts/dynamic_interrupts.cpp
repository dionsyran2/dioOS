#include <interrupts/interrupts.h>
#include <paging/PageTableManager.h>
#include <scheduling/spinlock/spinlock.h>
#include <memory/heap.h>
#include <memory.h>
#include <local.h>

spinlock_t lock = 0;

dynamic_isr_t* dynamic_isr_list = nullptr;
int dynamic_isr_ident = 0;

void add_dynamic_isr_to_list(dynamic_isr_t* isr){
    uint64_t rflags = spin_lock(&lock);
    asm ("cli");

    isr->identifier = dynamic_isr_ident;
    isr->next = nullptr;

    dynamic_isr_ident++;

    // If the list is empty...
    if (dynamic_isr_list == nullptr){
        dynamic_isr_list = isr;

        spin_unlock(&lock, rflags);
        return;
    }

    // Otherwise append it
    dynamic_isr_t* c = dynamic_isr_list;
    while (c->next) c = c->next;
    c->next = isr;

    spin_unlock(&lock, rflags);
}

void remove_dynamic_isr(dynamic_isr_t* isr){
    bool intr = are_interrupts_enabled();
    uint64_t rflags = spin_lock(&lock);
    asm ("cli");

    if (isr == dynamic_isr_list) {
        dynamic_isr_list = isr->next;
        return;
    }
    dynamic_isr_t* prev = nullptr;
    for (dynamic_isr_t* c = dynamic_isr_list; c != nullptr; c = c->next){
        if (c == isr){
            prev->next = c->next;
            break;
        }
        prev = c;
    }

    spin_unlock(&lock, rflags);
    if (intr) asm ("sti");
}

extern "C" void isr_dispatch(uint64_t vector){
    uint64_t old_cr3 = 0;
    
    for (dynamic_isr_t* isr = dynamic_isr_list; isr != nullptr; isr = isr->next){
        if (isr->vector == vector){
            isr->isr(isr->context);
        }
    }

    EOI();
}

extern "C" void* isr_stub_table_ptrs[256];

int _add_dynamic_isr(uint8_t vector, dynamic_isr_handler_t handler, void* context){
    // Create the isr structure
    dynamic_isr_t* isr = new dynamic_isr_t();
    isr->isr = handler;
    isr->context = context;
    isr->vector = vector;

    // Add it to the list
    add_dynamic_isr_to_list(isr);

    // Set the stub as the ISR in the IDT table
    _set_bsp_interrupt_service_routine(isr_stub_table_ptrs[vector], vector, IDT_TA_InterruptGate, 0x08);

    // Return the identifier (So we can remove the handler if necessary)
    return isr->identifier;
}

bool _remove_dynamic_isr(int identifier){
    for (dynamic_isr_t* isr = dynamic_isr_list; isr != nullptr; isr = isr->next){
        if (isr->identifier == identifier){
            remove_dynamic_isr(isr);
            delete isr;
            return true;
        }
    }

    return false;
}