#include "panic.h"
#include "BasicRenderer.h"
void panic(const char* panicMsg){
    globalRenderer->Clear(0x004f57);

    globalRenderer->cursorPos.X = 0;
    globalRenderer->cursorPos.Y = 0;

    globalRenderer->print("Kernel Panic\n\n");

    globalRenderer->print(panicMsg);
    while(1);
}