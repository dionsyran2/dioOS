#pragma once
#include <stdint.h>
#include <scheduling/task/scheduler.h>

namespace taskScheduler{
    struct task_t;
}
void RunTaskInUserMode(taskScheduler::task_t* task);
extern "C" void flush_tss();
extern "C" void jump_usermode(uint64_t stack, uint64_t function, uint64_t parameter);