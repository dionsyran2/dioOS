#include <syscalls/syscalls.h>
#include <memory.h>
#include <helpers/elf.h>
#include <syscalls/files/helpers.h>
#include <alloca.h>

#define MAX_ARG_STRLEN 4096

long sys_execve(const char *pathname, char *const argv[], char *const envp[]){
    task_t* self = task_scheduler::get_current_task();
    char path[2048];
    fix_path(pathname, path, sizeof(path), self);

    // Count the length of argv
    int argc = 0;
    while(1){
        uintptr_t arg_address = 0;
        self->read_memory((char*)argv + (argc * sizeof(uintptr_t)), &arg_address, sizeof(uintptr_t));

        if (arg_address == 0) break;

        argc++;
    }

    // Count the length of envp
    int envc = 0;
    while(1){
        uintptr_t env_address = 0;
        self->read_memory((char*)envp + (envc * sizeof(uintptr_t)), &env_address, sizeof(uintptr_t));

        if (env_address == 0) break;

        envc++;
    }

    // Copy the variables into the kernel space
    const char *kargv[argc + 1] = {};
    const char *kenvp[envc + 1] = {};
    
    for (int i = 0; i < argc; i++){
        uintptr_t arg_address = 0;
        self->read_memory((char*)argv + (i * sizeof(uintptr_t)), &arg_address, sizeof(uintptr_t));

        if (arg_address == 0) break;

        char* heap_buffer = self->read_string((void*)arg_address, MAX_ARG_STRLEN);
        char* stack_buffer = (char*)alloca(strlen(heap_buffer) + 1);
        strcpy(stack_buffer, heap_buffer);

        free(heap_buffer);
        kargv[i] = stack_buffer;
    }

    for (int i = 0; i < envc; i++){
        uintptr_t env_address = 0;
        self->read_memory((char*)envp + (i * sizeof(uintptr_t)), &env_address, sizeof(uintptr_t));

        if (env_address == 0) break;

        char* heap_buffer = self->read_string((void*)env_address, MAX_ARG_STRLEN);
        char* stack_buffer = (char*)alloca(strlen(heap_buffer) + 1);
        strcpy(stack_buffer, heap_buffer);

        free(heap_buffer);
        kenvp[i] = stack_buffer;
    }

    return kexecve(path, argc, kargv, envc, kenvp);
}

REGISTER_SYSCALL(SYS_execve, sys_execve);