#include "panic.h"
#include "drivers/serial.h"
#include "BasicRenderer.h"
void panic(const char* panicMsg, const char* debug){
    asm ("cli");
    serialPrint(COM1, "KERNEL PANIC\n\r\033");
    serialPrint(COM1, panicMsg);
    serialPrint(COM1, "\n\r\033");
    serialPrint(COM1, "DEBUG:: ");
    serialPrint(COM1, debug);

    globalRenderer->Set(true);
    for (int t = 0; t > 1000; t++);
    globalRenderer->Clear(0x0f0024);

    globalRenderer->cursorPos.X = 0;
    globalRenderer->cursorPos.Y = 0;

    globalRenderer->printf("Kernel Panic\n\n");

    globalRenderer->printf("%s\nDEBUG:: %s", panicMsg, debug);
    while(1);
}