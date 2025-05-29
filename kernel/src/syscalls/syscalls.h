#pragma once
#include <stdint.h>
#include "../drivers/storage/volume/volume.h"
#include "../scheduling/apic/apic.h"
#include "../drivers/rtc/rtc.h"
#include "../other/ELFLoader.h"

extern "C" void sys_free_cpp(uint64_t address);
extern "C" void* sys_malloc_cpp(uint64_t size);
extern "C" void syscall_int_handler();
extern "C" void* sys_open_cpp(char* path);
extern "C" void* sys_load_cpp(DirEntry* entry);
extern "C" void sys_close_file_cpp(DirEntry* entry);
extern "C" void* sys_get_framebuffer_cpp();
extern "C" void* sys_request_pages_cpp(uint64_t pages);
extern "C" void sys_free_pages_cpp(void* addr, uint64_t count);
extern "C" void setInputEvent(uint64_t val);
extern "C" void sys_set_tmr_val_cpp(RTC::rtc_time_t* time);
extern "C" void sys_add_int_cpp(void* hndlr, uint8_t intr);
extern "C" void sys_add_elf_task_cpp(DirEntry* entry);
extern "C" void sys_task_sleep_cpp(uint64_t time);
extern "C" taskScheduler::task_t* sys_create_thread_cpp(void* task, taskScheduler::task_t* parent);
extern "C" taskScheduler::task_t* sys_get_current_task_cpp();
extern "C" uint64_t sys_get_ticks_cpp();
extern "C" void sys_log_cpp(const char* str);
extern "C" void sys_reg_query_cpp(char* keyPath, char* valueName, void* data);

extern "C" void sys_power_cpp(int state);

extern "C" int sys_create_pipe_cpp(char* name);
extern "C" void sys_destroy_pipe_cpp(int hdnl);
extern "C" int sys_get_pipe_cpp(char* name);
extern "C" void sys_write_pipe_cpp(int hndl, char* data);
extern "C" char* sys_read_pipe_cpp(int hndl);
extern "C" DirectoryList* sys_get_dir_list(char drive, DirEntry* directory);