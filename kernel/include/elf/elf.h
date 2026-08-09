#pragma once
#include <scheduling/task_scheduler/task_scheduler.h>
#include <vfs/vfs.h>

#define PUSH_TO_STACK(task, rsp, type, value) do {       \
    type __temp = (value);                               \
    rsp -= sizeof(type);                                 \
    (task)->write_to_userspace((void*)(rsp), &__temp, sizeof(type)); \
} while(0)

int load_elf(task_t *task, vnode_t *node, int argc, char *argv[], const char *exec_path);