#include "taskScheduler.h"
#include "../../memory/heap.h"

void TaskScheduler::Init(){
    CPU cpu = get_cpu_info();
    numOfCores = cpu.coresPerPackage;
    numOfThreads = cpu.logicalProcessors;
}


uint8_t TaskScheduler::GetLeastUsedCore(){
    int64_t MinCore = 0;
    uint8_t MinCoreIndex = 0;
    for (int i = 0; i < numOfCores; i++){
        if (Tasks[i] == 0) return i;
        if (MinCore == 0){
            MinCore = Tasks[i];
            MinCoreIndex = i;
            continue;
        }
        if (Tasks[i] < MinCore){
            MinCore = Tasks[i];
            MinCoreIndex = i;
        }
    }
    return MinCoreIndex;
}
void TaskScheduler::AddTaskToScheduler(Task* task) {
    uint8_t priority = task->priority;
    if (taskCount[priority] < MAX_TASKS) {
        taskQueue[priority][taskCount[priority]] = task;
        taskCount[priority]++;
    }
}
Task* TaskScheduler::CreateTask(void (*function)(void), uint8_t priority){
    GetLeastUsedCore();
    uint8_t cpu = GetLeastUsedCore();
    Task* newTask = (Task*)malloc(sizeof(Task));
    newTask->function = function;
    newTask->assignedCPU = cpu;
    newTask->state = TaskState::Waiting;
    newTask->priority = priority;
    AddTaskToScheduler(newTask);
    numOfTasks++;
    Tasks[cpu]++;
    return newTask;
}


Task* TaskScheduler::GetHighestPriorityTask() {
    for (int priority = PRIORITY_LEVELS; priority >= 0; priority--) { //Count down so we get the highest priority first!
        if (taskCount[priority] > 0 ) {
            for (int y = 0; y < taskCount[priority]; y++){
                if (!(taskQueue[priority][y]->state == TaskState::Running || taskQueue[priority][y]->state == TaskState::Finished)){
                    return taskQueue[priority][y];
                }
            }
        }
    }
    return NULL; //No tasks
}

void TaskScheduler::RemoveTaskFromScheduler(Task* task) {
    uint8_t priority = task->priority;

    for (int i = 0; i < taskCount[priority]; i++) {
        if (taskQueue[priority][i] == task) {
            for (int j = i; j < taskCount[priority] - 1; j++) {
                taskQueue[priority][j] = taskQueue[priority][j + 1];
            }
            taskCount[priority]--;
            break;
        }
    }
}

void save_task_state(Task* currentTask) {
    // Save the general-purpose registers
    asm volatile (
        "mov %%rsp, %0\n"        // Save the stack pointer
        "mov %%rbp, %1\n"        // Save the base pointer
        "mov %%rax, %2\n"        // Save other general-purpose registers
        "mov %%rbx, %3\n"
        "mov %%rcx, %4\n"
        "mov %%rdx, %5\n"
        : "=m"(currentTask->rsp), "=m"(currentTask->rbp),
          "=m"(currentTask->rax), "=m"(currentTask->rbx),
          "=m"(currentTask->rcx), "=m"(currentTask->rdx)
        :
        : "memory"
    );

    // Save flags (RFLAGS)
    asm volatile (
        "pushfq\n"                // Push RFLAGS to the stack
        "pop %0\n"                // Pop it into the task structure
        : "=m"(currentTask->rflags)
        :
        : "memory"
    );

    // Save the instruction pointer (RIP) using a label trick
    uint64_t rip_value;
    asm volatile (
        "lea 1f(%%rip), %0\n"   // Save the address of the next instruction (RIP)
        "1:\n"
        : "=r"(rip_value)       // Use a general-purpose register to store the RIP value
        :
        : "memory"
    );
    currentTask->rip = rip_value;
}


void restore_task_state(Task* nextTask) {
    // Restore the general-purpose registers
    asm volatile (
        "mov %0, %%rsp\n"        // Restore the stack pointer
        "mov %1, %%rbp\n"        // Restore the base pointer
        "mov %2, %%rax\n"        // Restore other general-purpose registers
        "mov %3, %%rbx\n"
        "mov %4, %%rcx\n"
        "mov %5, %%rdx\n"
        :
        : "m"(nextTask->rsp), "m"(nextTask->rbp),
          "m"(nextTask->rax), "m"(nextTask->rbx),
          "m"(nextTask->rcx), "m"(nextTask->rdx)
        : "memory"
    );

    // Restore flags (RFLAGS)
    asm volatile (
        "push %0\n"              // Push RFLAGS onto the stack
        "popfq\n"                // Pop it into the flags register
        :
        : "m"(nextTask->rflags)
        : "memory"
    );

    // Jump to the next task's instruction pointer (RIP)
    asm volatile (
        "jmp *%0\n"              // Jump to the saved RIP (resume task)
        :
        : "m"(nextTask->rip)
        : "memory"
    );
}


void TaskScheduler::ContextSwitch(Task* current, Task* next) {
    // Save current task's state (registers, stack pointer, etc.)
    save_task_state(current);
    current->state = TaskState::Blocked;
    currentTask = next;
    // Restore next task's state
    restore_task_state(next);
    next->state = TaskState::Running;
}
void idle_task() {
    while (1) {
        // Just loop infinitely, doing nothing
        asm volatile("hlt");  // Halt CPU until the next interrupt
    }
}
void TaskScheduler::schedule() {
    Task* highestPriorityTask = GetHighestPriorityTask();
    if (highestPriorityTask == NULL) {
        return;
    }
    //send_ipi(highestPriorityTask->assignedCPU, 0x41);
    if (currentTask == NULL) {
        // If no task is currently running (first task being scheduled)
        currentTask = highestPriorityTask;
        restore_task_state(currentTask);  // Just restore and run the first task
    } else if (highestPriorityTask && highestPriorityTask != currentTask) {
        ContextSwitch(currentTask ,highestPriorityTask);
    }
}
