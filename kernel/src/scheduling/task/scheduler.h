#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../apic/apic.h"
#include "../../paging/PageFrameAllocator.h"
#include "../../userspace/userspace.h"
#include "../../kernel.h"

#define TASK_SCHEDULER_COUNTER_DEFAULT 20//The larger the quantum, the "laggier" the user experience - quanta above 100ms should be avoided.
namespace taskScheduler{
    typedef void (*func_ptr)();

    enum task_state_t{
        Stopped = 0,
        Running = 1,
        Paused = 2,
        Blocked = 3,
    };

    struct task_t{
        uint64_t pid;
        bool valid = false;
        bool exit = false; // Whether an exit was requested. if yes, any reserved memory will be unreserved and the task will be removed from the list.   
        char task_name[32];
        task_state_t state = Stopped;
        uint64_t stack = 0;
        uint64_t kernelStack = 0;
        uint8_t tasklist = 0;
        uint64_t rbp;
        uint64_t rsp;
        uint64_t rip;
        uint8_t blockType = 0; //1 = Sleep, 2 = iowait.
        uint64_t block = 0;
        uint8_t priotity;
        bool reducedCounter = 0; //If set, reduces the counter to 5ms
        bool userspace;
        void initialize(void* function);
        void setName(char* name);
        void decreaseCounter();
        void resetCounter();
        void Sleep();
        uint64_t getCounter();
        bool isUnblocked();
        func_ptr function;
        task_t* parent;
        PageTableManager* ptm;
        uint64_t tmp_PML4;
        PageTableManager* CreatePageTableMgr();
        private:
        //uint8_t priority; // used in case of priority switching, so the thread is able to get some cpu time if it is starved by higher priority processes.
        int64_t counter = 0;
    };

    struct tasklist_t{
        task_t* tasks[1024];
        size_t numOfTasks = 0;
    };
    
    void InitializeScheduler(uint8_t numOfPriotities);
    task_t* CreateTask(void* task, uint8_t priority, bool user);
    void RemoveTask(task_t* task);
    void SwitchTask(uint64_t rbp, uint64_t rsp);
    void SchedulerTick(uint64_t rbp, uint64_t rsp);
    extern tasklist_t* tasklists;
    extern uint8_t numOfTaskLists;
    extern bool disableSwitch;
    extern task_t* currentTask;
    extern task_t* WM_TASK;
}
extern "C" void run_task_asm();
