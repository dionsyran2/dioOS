#include "kernel.h"
#define BOOT_SCREEN
#include "boot_screen.h"

/*
TODO: Rewrite everything that is memory related
TODO: Rewrite driver subsystem
*/



extern "C" void _main(uint32_t magic, multiboot_info* mb_info_addr){
    InitKernel(magic, mb_info_addr);
    flush_tss();

    taskScheduler::InitializeScheduler(5);    

    taskScheduler::CreateTask((void*)SecondaryKernelInit, 0, false)->setName("Kernel Initialization");
    taskScheduler::CreateTask((void*)bootTest, 0, false)->setName("Boot animation");
    SecondaryKernelInit();
    //taskScheduler::SwitchTask(0, 0); // Initiate the scheduler... this will make it run the first task.

    //(shouldn't return to _main, since control has been handed over to the scheduler... and _main is not a scheduled task)
    kprintf("Returned to _main\n");
    
    while(1){
        asm ("nop");
    }
}