#pragma once
#include <stdint.h>
#include <stddef.h>
#include "memory.h"
#include "cstr.h"
#include "ArrayList.h"
#include "paging/PageFrameAllocator.h"

typedef int pipehndl;

struct pipe{
    int hndl;
    char name[50];
    char* buffer;

    bool writting = false;
    bool reading = false;

    int rp;
    int wp;
};



pipehndl CreatePipe(char* name);

pipehndl GetPipe(char* name);

pipe* GetPipe(pipehndl hndl);

void DestroyPipe(pipehndl hndl);

int GetReadPointer(pipehndl hndl);

int GetWritePointer(pipehndl hndl);


char* ReadPipe(pipehndl hndl);

void WritePipe(pipehndl hndl, char* wr);

void ClearPipe(pipehndl hndl);