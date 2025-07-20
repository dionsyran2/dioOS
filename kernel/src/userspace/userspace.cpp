#include <userspace/userspace.h>

void TaskUserspaceWrapper(task_t* task){

    __asm__ __volatile__("mov %0, %%rdi" :: "r" (task->rdi));
    __asm__ __volatile__("mov %0, %%rsi" :: "r" (task->rsi));
    __asm__ __volatile__("mov %0, %%rdx" :: "r" (task->rdx));
    uint64_t stack = task->rsp ? task->rsp : (task->stack + TASK_SCHEDULER_DEFAULT_STACK_SIZE);
    stack &= ~0xF;
    
    __asm__ __volatile__("mov %0, %%rsp" :: "r" (stack));
    
    task_scheduler::disable_scheduling = false;
    if (task->is_signal_handler){
        task->entry();
        __asm__ __volatile__ ("mov $15, %rax");
        __asm__ __volatile__ ("syscall");
    }else{
        __asm__ __volatile__("jmp *%0" :: "r" (task->entry));
    }

    while (1); 
}

void RunTaskInUserMode(task_t* task){
    task_scheduler::disable_scheduling = true;
    __asm__ __volatile__("mov %0, %%rsp" :: "r" (task->kstack + TASK_SCHEDULER_DEFAULT_STACK_SIZE));

    uint64_t stack = task->rsp ? task->rsp : (task->stack + TASK_SCHEDULER_DEFAULT_STACK_SIZE);
    stack &= ~0xF;
    //kprintf("stack: %llx\n", stack);

    jump_usermode(stack, (uint64_t)TaskUserspaceWrapper, (uint64_t)task);
    while(1);
}