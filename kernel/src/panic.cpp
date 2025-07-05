#include <panic.h>
#include <drivers/serial.h>
#include <BasicRenderer.h>
#include <scheduling/task/scheduler.h>
#include <kernel.h>

void panic(const char* panicMsg, const char* debug){
    asm ("cli");

    //taskScheduler::disableSwitch = true;
    globalRenderer->Set(true);
    for (int t = 0; t > 1000; t++);
    globalRenderer->Clear(0x0f0024);

    globalRenderer->cursorPos.X = 0;
    globalRenderer->cursorPos.Y = 0;

    kprintf("Kernel Panic\n\n");

    kprintf("%s\nDEBUG::\n", panicMsg);
    kprintf(
        0xFFFF00,
        "Total system memory: %d MB\nSystem Reserved Memory: %d\nUsed Memory: %d\nFree Memory:%d\n",
        (GlobalAllocator.GetMemSize()/1024)/1024,
        (GlobalAllocator.GetReservedRAM()/1024)/1024,
        (GlobalAllocator.GetUsedRAM()/1024)/1024,
        (GlobalAllocator.GetFreeRAM()/1024)/1024
    );
    
    kprintf(
        0xFFFF00,
        "Total kheap memory: %d KB\nUsed kheap memory: %d KB\nFree kheap memory: %d KB\n",
        heapSize / 1024,
        heapUsed / 1024,
        heapFree / 1024
    );

    kprintf("%s", debug);

    Sleep(15000);

    Restart();
    
    while(1) asm volatile ("hlt");
}