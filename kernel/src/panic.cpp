#include <panic.h>
#include <kstdio.h>
#include <scheduling/apic/lapic.h>
#include <interrupts/interrupts.h>
#include <local.h>

void panic(const char* str, ...){
    va_list args;
    va_start(args, str);

    kprintf("\e[41;30mKERNEL PANIC!\e[0m\n");
    kprintfva(str, args);
    
    va_end(args);

    //task_t *self = task_scheduler::get_current_task();
    cpu_local_data* local = get_cpu_local_data();

    kprintf("CPU Logical ID: %d\n", local ? local->cpu_id : -1);
    //kprintf("Running Task: %s (%d)\n", self ? self->name : "NONE", self ? self->pid : -1);

    __asm__ ("cli");
    while(1) __asm__ ("hlt");
}