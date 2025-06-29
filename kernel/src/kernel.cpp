#include "kernel.h"
#define BOOT_SCREEN
#include <boot_screen.h>
#include <scheduling/lock/spinlock.h>
#include <VFS/vfs.h>


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

    taskScheduler::InitializeScheduler(5);    

    taskScheduler::task_t* task = taskScheduler::CreateTask((void*)SecondaryKernelInit, 0, false);
    task->setName("Kernel Initialization");
    task->valid = true;

    taskScheduler::task_t* task2 = taskScheduler::CreateTask((void*)bootTest, 0, false);
    task2->setName("Boot animation");
    task2->valid = true;
    //SecondaryKernelInit();

    setup_syscalls();
    taskScheduler::SwitchTask(0, 0); // Initiate the scheduler... this will make it run the first task.


    //(shouldn't return to _main, since control has been handed over to the scheduler... and _main is not a scheduled task)
    
    kprintf("Returned to _main\n");

    while(1){
        asm ("nop");
    }
}