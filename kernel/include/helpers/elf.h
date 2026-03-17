#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/task_scheduler/task_scheduler.h>

#define PUSH_TO_STACK(task, rsp, type, value) do {       \
    type __temp = (value);                               \
    rsp -= sizeof(type);                                 \
    (task)->write_memory((void*)(rsp), &__temp, sizeof(type)); \
} while(0)

task_t* execute_elf(vnode_t* node, int argc, char* argv[], char* exec_path);

int kexecve(const char* pathname, int argc, const char* argv[], int envc, const char* envp[]);