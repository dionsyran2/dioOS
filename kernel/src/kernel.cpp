#include <kernel.h>
#define BOOT_SCREEN
#include <boot_screen.h>
#include <scheduling/lock/spinlock.h>
#include <VFS/vfs.h>
#include <scheduling/task/scheduler.h>
#include <userspace/userspace.h>

/* AP MAIN */
/*
SMP TEMPORARILY DISABLED...
and the old ugly scheduler is back...
*/
extern "C" void _apmain(){
    ap_started = true; // Toggle the flag to signal that the ap has started

    SetupAP();

    InitLAPIC();

    //scheduler::InitCPU(); // Add the ap to the scheduler

    asm ("sti");

    SetupAPICTimer();

    while(1);
}

/* BSP / KERNEL MAIN */
extern "C" void _main(uint32_t magic, multiboot_info* mb_info_addr){
    InitKernel(magic, mb_info_addr);
    flush_tss();

    task_scheduler::init();   
     

    task_t* task = task_scheduler::create_process("Kernel Initialization", SecondaryKernelInit);
    task->flags |= SYSTEM_TASK;
    task_scheduler::mark_ready(task);

    task_t* task2 = task_scheduler::create_process("Boot animation", bootTest);
    task->flags |= SYSTEM_TASK;
    task_scheduler::mark_ready(task2);

    setup_syscalls();
    task_scheduler::disable_scheduling = false;
    task_scheduler::run_next_task(0, 0);
    //(shouldn't return to _main, since control has been handed over to the scheduler... and _main is not a scheduled task)
    
    kprintf("Returned to _main\n");

    while(1){
        asm ("nop");
    }
}