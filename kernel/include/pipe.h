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


char* ReadPipe(pipehndl hndl, size_t* out_length, bool null_terminate);

void WritePipe(pipehndl hndl, const char* wr, size_t length);

void ClearPipe(pipehndl hndl);