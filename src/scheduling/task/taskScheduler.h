#pragma once
#include "../../cpu.h"
#include "../apic/apic.h"
#define PRIORITY_LEVELS 254  // Number of priority levels (0-255)
#define MAX_TASKS 1024       // Max tasks in the system



enum TaskState{
    Running = 0,//Running
    Waiting = 1,//Waiting in Queue
    Finished = 2,//Finished Execution
    Blocked = 3//Waiting for IO input or an event
};

struct Task {
    uint32_t id;                // Task ID
    void* stackPointer;         // Pointer to task's stack
    void (*function)(void);     // Function to be executed
    TaskState state;              // State of the task (running, waiting, etc.)
    uint8_t assignedCPU;        // Which CPU or thread this task is assigned to (if specific)
    uint8_t priority;
    uint64_t rsp;    // Stack pointer
    uint64_t rbp;    // Base pointer
    uint64_t rip;    // Instruction pointer
    uint64_t rflags; // Flags register
    uint64_t rax, rbx, rcx, rdx; // General-purpose registers
};

struct TaskScheduler{
    public:
    uint32_t numOfCores;
    uint32_t numOfThreads;
    uint32_t numOfTasks;
    uint64_t Tasks[50];
    void Init();
    Task* CreateTask(void (*function)(void), uint8_t priority);
    uint8_t GetLeastUsedCore();
    int taskCount[PRIORITY_LEVELS]; 
    Task* taskQueue[PRIORITY_LEVELS][MAX_TASKS];
    Task* GetHighestPriorityTask();
    void AddTaskToScheduler(Task* task);
    void RemoveTaskFromScheduler(Task* task);
    void schedule();
    void ContextSwitch(Task* current, Task* next);
    Task* currentTask;
};