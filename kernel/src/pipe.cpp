#include "pipe.h"
ArrayList<pipe*>* pipes = nullptr;
int lhndl = 1;

pipehndl CreatePipe(char* name){
    if (pipes == nullptr) pipes = new ArrayList<pipe*>();
    pipe* p = new pipe;
    strcpy(p->name, name);

    p->buffer = (char*)GlobalAllocator.RequestPage();
    memset(p->buffer, 0, 0x1000);

    p->hndl = lhndl;
    lhndl++;
    p->rp = 0;
    p->wp = 0;

    pipes->add(p);
    return p->hndl;
}

pipehndl GetPipe(char* name){
    for (size_t i = 0; i < pipes->length(); i++){
        pipe* p = pipes->get(i);
        if (strcmp(p->name, name) == 0){
            return p->hndl;
        } 
    }
    return -1;
}

pipe* GetPipe(pipehndl hndl){
    for (size_t i = 0; i < pipes->length(); i++){
        pipe* p = pipes->get(i);
        if (p->hndl == hndl){
            return p;
        } 
    }
    return nullptr;
}

void DestroyPipe(pipehndl hndl){
    pipe* p = GetPipe(hndl);
    if (p == nullptr) return;
    pipes->remove(p);
}

int GetReadPointer(pipehndl hndl){
    pipe* p = GetPipe(hndl);
    if (p == nullptr) return 0;
    return p->rp;
}

int GetWritePointer(pipehndl hndl){
    pipe* p = GetPipe(hndl);
    if (p == nullptr) return 0;
    return p->wp;
}


char* ReadPipe(pipehndl hndl){
    pipe* p = GetPipe(hndl);
    if (p == nullptr) return nullptr;

    while (p->writting) { __asm__ volatile ("pause"); }

    p->reading = true;
    int rp = GetReadPointer(hndl);
    int wp = GetWritePointer(hndl);

    if (rp == wp) {
        p->reading = false;
        return nullptr;
    }
    if (rp < wp){
        int amount_to_read = wp - rp;
        char* alloc = new char[amount_to_read + 1];

        memcpy(alloc, p->buffer + rp, amount_to_read * sizeof(char));
        alloc[amount_to_read] = '\0';

        rp = wp;
        p->reading = false;
        p->rp = rp;
        return alloc;
    }else{
        int amount_to_read = (0x1000 - rp) + wp;

        char* alloc = new char[amount_to_read + 1];

        memcpy(alloc, p->buffer + rp, 0x1000 - rp);
        memcpy(alloc + (0x1000 - rp), p->buffer, wp);
        alloc[amount_to_read] = '\0';
        rp = wp;
        p->reading = false;
        p->rp = rp;
        return alloc;
    }

    p->reading = false;
    return nullptr;
}

void WritePipe(pipehndl hndl, char* wr){
    pipe* p = GetPipe(hndl);
    if (p == nullptr) return;
    while (p->reading) { __asm__ volatile ("pause"); }

    p->writting = true;

    int length = strlen(wr);
    
    while (length > 0){
        if (p->wp >= 0x1000) p->wp = 0;

        p->buffer[p->wp] = *wr;
        p->wp++;
        length--;
        wr++;
    }
    p->writting = false;
}

void ClearPipe(pipehndl hndl){
    pipe* p = GetPipe(hndl);
    if (p == nullptr) return;

    p->wp = 0;
    p->rp = 0;
    p->reading = false;
    p->writting = false;
    memset(p->buffer, 0, 0x1000);
}