#include "syscall.h"
#include "../BasicRenderer.h"
#include "../memory/heap.h"
#include "../drivers/storage/volume/volume.h"
#include "../UserInput/inputClientEvents.h"
#include "../drivers/rtc/rtc.h"
#include "../scheduling/apic/apic.h"
#include "../kernel.h"

extern "C" void HandleSyscall(){
    globalRenderer->printf("SYSCALL\n");
}

extern "C" void sys_free_cpp(uint64_t address){
    free((void*)address);
}

extern "C" void* sys_open_cpp(char disk, char* name, void* dir){
    for (int i = 0; i < VolumeSpace::numOfVolumes; i++){
        if (VolumeSpace::volumes[i].assignedLetter == disk){
            return VolumeSpace::volumes[i].OpenFile(name, (DirEntry*)dir);
        }
    }
    return nullptr;
}

extern "C" void* sys_load_cpp(DirEntry* entry){
    if (entry == nullptr) return nullptr;
    return entry->volume->LoadFile(entry);
}

extern "C" void* sys_malloc_cpp(uint64_t size){
    return malloc(size);
}

extern "C" void* sys_get_framebuffer_cpp(){
    return globalRenderer->targetFramebuffer;
}

extern "C" void* sys_request_pages_cpp(uint64_t pages){
    if (pages == 1) return GlobalAllocator.RequestPage();
    return GlobalAllocator.RequestPages(pages);
}

extern "C" void sys_free_pages_cpp(void* addr, uint64_t count){
    if (count == 1) return GlobalAllocator.FreePage(addr);
    GlobalAllocator.FreePages(addr, count);
}

extern "C" void setInputEvent(uint64_t val){
    InEvents::EVENT_BUFFER = (void**)val;
}

extern "C" void sys_set_tmr_val_cpp(RTC::rtc_time_t* time){
    c_time = time;
}

extern "C" void sys_add_int_cpp(void* hndlr, uint8_t intr){
    SetIDTGate(hndlr, intr, IDT_TA_InterruptGate_U, 0x8);
}

extern "C" void sys_add_elf_task_cpp(DirEntry* entry){
    taskScheduler::disableSwitch = true;
    ELFLoader::Load(entry, 10);
    taskScheduler::disableSwitch = false;
}

extern "C" void sys_task_sleep_cpp(uint64_t time){
    taskScheduler::currentTask->blockType = 1;
    taskScheduler::currentTask->block = GetAPICTick() + time;
    while(taskScheduler::currentTask->getCounter() > 1){
        taskScheduler::currentTask->decreaseCounter();
    }
}

extern "C" taskScheduler::task_t* sys_create_thread_cpp(void* task, taskScheduler::task_t* parent){
    taskScheduler::task_t* thread = taskScheduler::CreateTask(task, 10, true);
    thread->parent = parent;
    thread->ptm = parent->ptm;
    return thread;
}

extern "C" taskScheduler::task_t* sys_get_current_task_cpp(){
    return taskScheduler::currentTask;
}