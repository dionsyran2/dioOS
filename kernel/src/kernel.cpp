#include "kernel.h"

/*
TODO: Write code for the relocation of the kernel code to a high address.
TODO: Only map the addresses needed in the page table, dont map 1:1 the entire memory
TODO: Make the scheduler manage virtual address spaces, and add a table with all of the allocated addresses so we can free it when the application closes (if the application hasnt already)
TODO: Fix the elf loader so it will create a virtual address space and correctly load all of the sections, not just the code (that is at offset 0)
*/

extern "C" void _main(uint32_t magic, multiboot_info* mb_info_addr){
    InitKernel(magic, mb_info_addr);

    flush_tss();





    taskScheduler::InitializeScheduler(5);

    DirEntry* apps = VolumeSpace::volumes[0].OpenFile("applications", nullptr);
    DirEntry* entry = VolumeSpace::volumes[0].OpenFile("WM      ELF", apps);
    if (entry == nullptr){
        panic("wm.exe could not be located!", "");
    }
    
    ELFLoader::Load(entry, 0);    



    
    
    taskScheduler::SwitchTask(0, 0);//Initiate the scheduler... this will make it run the first task.






    //(shouldn't return to _main, since control has been handed over to the scheduler... and _main is not a scheduled task)
    globalRenderer->printf("Returned to _main\n");
    
    while(1){
        asm ("nop");
    }
}