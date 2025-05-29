#include "scheduler.h"

namespace taskScheduler{
    bool disableSwitch = false;
    tasklist_t* tasklists;
    uint8_t numOfTaskLists;
    uint64_t procID = 0;
    task_t* WM_TASK;

    void IdleTask(){
        while(1);
    }

    void task_t::decreaseCounter(){
        if (this->counter > 0) this->counter--;
    }
    
    uint64_t task_t::getCounter(){
        return this->counter;
    }
    void task_t::resetCounter(){
        if (reducedCounter == true){
            this->counter = 5;
        }else{
            this->counter = TASK_SCHEDULER_COUNTER_DEFAULT;
        }
    }

    bool task_t::isUnblocked(){
        switch (this->blockType){
            case 1: //Sleep
                return (GetAPICTick() > this->block) ? true : false;
            case 2: //IO Wait
                io_wait(); //idk how to implement that yet
                return true;
            default:
                return true;
        }
    }

    void task_t::setName(char* name){
        strcpy((char*) &this->task_name, name);
    }

    void task_t::initialize(void* function){
        this->function = (func_ptr)function;
        this->stack = (uint64_t)GlobalAllocator.RequestPages((4 * 1024 * 1024) / 0x1000); //40 kb
        this->kernelStack = (uint64_t)GlobalAllocator.RequestPages((4 * 1024 * 1024) / 0x1000); //40 kb
        this->state = Stopped;
        this->valid = true;
        this->resetCounter();
        this->setName("Unnamed Task");
        this->pid = procID;
        procID++;
    }

    task_t* CreateTask(void* function, uint8_t priority, bool user){
        if (priority > (numOfTaskLists - 1)) priority = numOfTaskLists - 1;
        task_t* task = new task_t;

        task->initialize(function);
        task->userspace = user;
        task->tasklist = priority;
        task->ptm = nullptr;
        task->tmp_PML4 = NULL;
        tasklists[priority].tasks[tasklists[priority].numOfTasks] = task;
        tasklists[priority].numOfTasks++;
        return task;
    }

    PageTableManager* task_t::CreatePageTableMgr(){
        ptm = new PageTableManager((PageTable*)GlobalAllocator.RequestPage());
        memset(ptm->PML4, 0, 0x1000);
        ptm->MapMemory(ptm->PML4, ptm->PML4);

        globalPTM.ClonePTM(ptm);
        return ptm;
    }

    void RemoveTask(task_t* task){
        if (task->valid == false) return;
        if (task->stack != 0){
            GlobalAllocator.FreePages((void*)task->stack, 10);
            GlobalAllocator.FreePages((void*)task->kernelStack, 10);
            if (task->ptm != nullptr){
                GlobalAllocator.FreePage(task->ptm->PML4);
                free(task->ptm);
            }
        }
        
        int index = 0;
        for (int i = 0; i < tasklists[task->tasklist].numOfTasks; i++){
            if (tasklists[task->tasklist].tasks[i] == task){
                index = i;
                break;
            }
        }

        for (int i = index; i < tasklists[task->tasklist].numOfTasks; i++){
            tasklists[task->tasklist].tasks[i] = tasklists[task->tasklist].tasks[i + 1];
        }
        tasklists[task->tasklist].numOfTasks--;
        tasklists[task->tasklist].tasks[tasklists[task->tasklist].numOfTasks] = nullptr;
        task->valid = false;
    }

    void ResetAllCounters(){
        for (int p = 0; p < numOfTaskLists; p++){
            tasklist_t* list = &tasklists[p];
            uint16_t count = 0;
            for (int i = 0; i < 1024; i++){
                if (count >= list->numOfTasks) break;
                task_t* task = list->tasks[i];

                if (task->valid == false) continue;
                count++;
                task->resetCounter();

            }
        }
    }

    task_t* FindNextTask(){
        for (int p = 0; p < numOfTaskLists; p++){
            tasklist_t* list = &tasklists[p];
            
            for (int i = 0; i < list->numOfTasks; i++){
                task_t* task = list->tasks[i];

                if (task->valid == false) continue;
                if (task->state == Blocked && task->isUnblocked()){
                    return task;
                }
                if (task->getCounter() > 0) return task; //if the counter is not 0, it means that the task has not been run yet
            }
        }
        
        return nullptr;
    }

    task_t* GetNextTask(){
        task_t* task = FindNextTask();
        if (task == nullptr){
            //reset the counters for all of the tasks
            ResetAllCounters();
            task = FindNextTask();
        }

        if (task->exit == true){
            RemoveTask(task);
            return GetNextTask();
        }
        return task;
    }

    void InitializeScheduler(uint8_t numOfPriotities){
        void* buffer = GlobalAllocator.RequestPages(((sizeof(tasklist_t) * numOfPriotities) / 0x1000) + 1);
        memset(buffer, 0, (sizeof(tasklist_t) * numOfPriotities) + 0x1000);
        tasklists = (tasklist_t*)buffer;
        numOfTaskLists = numOfPriotities;

        task_t* idle = CreateTask((void*)&IdleTask, numOfPriotities - 1, false);
        idle->reducedCounter = true;
        idle->setName("Idle Task");
    }

    task_t* currentTask = nullptr;

    extern "C" void RunCurrentTask(){
        if (currentTask->ptm != nullptr){
            if (currentTask->tmp_PML4 != NULL){
                asm volatile ("mov %0, %%cr3" :: "r" (currentTask->tmp_PML4));
            }else{
                asm volatile ("mov %0, %%cr3" :: "r" (currentTask->ptm->PML4));
            }
        }else{
            asm volatile ("mov %0, %%cr3" :: "r" (globalPTM.PML4));
        }

        if (currentTask->state == Paused || currentTask->state == Blocked){ 
            /*if (currentTask->rsp > (currentTask->stack + (0x1000 * 10)) || currentTask->rsp < currentTask->stack){
                kprintf("Invalid rsp!\nRSP: %llx\nRBP: %llx\nSTACK: %llx", currentTask->rsp, currentTask->rbp, currentTask->stack);
                while(1);
            }

            if (currentTask->rbp > (currentTask->stack + (0x1000 * 10)) || currentTask->rbp < currentTask->stack){
                kprintf("Invalid rbp!\nRSP: %llx\nRBP: %llx\nSTACK: %llx", currentTask->rsp, currentTask->rbp, currentTask->stack);
                while(1);
            }*/
            asm volatile ("mov %0, %%rsp" :: "r" (currentTask->rsp));
            asm volatile ("mov %0, %%rbp" :: "r" (currentTask->rbp)); //These are the stack values from the interrupt.

            currentTask->state = Running;
            asm volatile ("jmp %0" :: "r" ((uint64_t)run_task_asm));
        }else{
            if (currentTask->userspace){
                RunTaskInUserMode(currentTask);
            }else{
                asm volatile ("sti");
                asm volatile ("mov %0, %%rsp" :: "r" (currentTask->stack + (4 * 1024 * 1024)));
                asm volatile ("mov %0, %%rdi" :: "r" (currentTask) : "rdi");
                
                currentTask->function();

                //task has ended (returned)
                currentTask->exit = true;
                while(1);
            }
            
        }
    }

    task_t* k_regs = nullptr;

    void SwitchTask(uint64_t rbp, uint64_t rsp){

        if (k_regs == nullptr){
            k_regs = new task_t;
            memset(k_regs, 0, sizeof(task_t));
            k_regs->valid = false;
        };
        
        task_t* nextTask = GetNextTask();
        if (currentTask != nullptr){
            if (currentTask->valid){
                //SaveCurrentTaskState();
                currentTask->rbp = rbp;
                currentTask->rsp = rsp;
                currentTask->state = Paused;

                if (currentTask->ptm != nullptr){
                    asm volatile ("mov %%cr3, %0" : "=r" (currentTask->tmp_PML4));
                }
            };
            
        }
        currentTask = nextTask;
        set_kernel_stack(nextTask->kernelStack + (0x1000 * 10));
        RunCurrentTask();

        while (1);
        
    }

    void SchedulerTick(uint64_t rbp, uint64_t rsp){
        if (currentTask != nullptr){
            if (currentTask->valid == false) return;
            if (disableSwitch) return;
            currentTask->decreaseCounter();
            if (currentTask->getCounter() <= 0){
                SwitchTask(rbp, rsp);
            }
        }
    }
}