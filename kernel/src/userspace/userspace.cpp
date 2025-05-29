#include "userspace.h"

void TaskUserspaceWrapper(taskScheduler::task_t* task){
    task->function();

    //task returned
    task->exit = true;
    while (1); 
}

void RunTaskInUserMode(taskScheduler::task_t* task){
    jump_usermode(task->stack + (0x1000 * 10), (uint64_t)TaskUserspaceWrapper, (uint64_t)task);
}