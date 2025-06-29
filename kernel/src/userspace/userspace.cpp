#include <userspace/userspace.h>

void TaskUserspaceWrapper(taskScheduler::task_t* task){

    __asm__ __volatile__("mov %0, %%rdi" :: "r" (task->rdi));
    __asm__ __volatile__("mov %0, %%rsi" :: "r" (task->rsi));
    __asm__ __volatile__("mov %0, %%rdx" :: "r" (task->rdx));
    uint64_t stack = task->rsp ? task->rsp : (task->stack + (4 * 1024 * 1024));
    stack &= ~0xF;
    __asm__ __volatile__("mov %0, %%rsp" :: "r" (stack));
    
    taskScheduler::disableSwitch = false;
    __asm__ __volatile__("jmp %0" :: "r" (task->function));

    while (1); 
}

void RunTaskInUserMode(taskScheduler::task_t* task){
    taskScheduler::disableSwitch = true;
    uint64_t stack = task->rsp ? task->rsp : (task->stack + (4 * 1024 * 1024));
    stack &= ~0xF;

    //kprintf("stack: %llx\n", stack);

    jump_usermode(stack, (uint64_t)TaskUserspaceWrapper, (uint64_t)task);
    while(1);
}