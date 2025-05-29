#include "syscall.h"
#include "../BasicRenderer.h"
#include "../memory/heap.h"
#include "../drivers/storage/volume/volume.h"
#include "../UserInput/inputClientEvents.h"
#include "../drivers/rtc/rtc.h"
#include "../scheduling/apic/apic.h"
#include "../kernel.h"
#include "../drivers/serial.h"
#include "../registry/registry.h"
#include "../pipe.h"

extern "C" void HandleSyscall(){
    kprintf("SYSCALL\n");
}

extern "C" void sys_free_cpp(uint64_t address){
    free((void*)address);
}

extern "C" void* sys_open_cpp(char* path){
    return VolumeSpace::OpenFile(path);
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

extern "C" uint64_t sys_get_ticks_cpp(){
    return APICticsSinceBoot;
}

extern "C" void sys_log_cpp(const char* str){
    serialPrint(COM1, str);
}

extern "C" void sys_reg_query_cpp(char* keyPath, char* valueName, void* data){
    RegQueryValue(keyPath, valueName, data);
}

extern "C" void sys_close_file_cpp(DirEntry* entry){
    entry->volume->CloseFile(entry);
}

extern "C" void sys_power_cpp(int state){
    if (state == 0) Shutdown();
    if (state == 1) Restart();
}

extern "C" int sys_create_pipe_cpp(char* name){
    return CreatePipe(name);
}

extern "C" void sys_destroy_pipe_cpp(int hndl){
    DestroyPipe(hndl);
}

extern "C" int sys_get_pipe_cpp(char* name){
    return GetPipe(name);
}

extern "C" void sys_write_pipe_cpp(int hndl, char* data){
    WritePipe(hndl, data);
}

extern "C" char* sys_read_pipe_cpp(int hndl){
    return ReadPipe(hndl);
}

extern "C" DirectoryList* sys_get_dir_list(char drive, DirEntry* directory){
    return VolumeSpace::getVolume(drive)->listDir(directory);
}