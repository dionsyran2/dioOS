#include <scheduling/task/scheduler.h>
#include <syscalls/syscalls.h>
#include <kernel.h>

namespace taskScheduler{
    bool disableSwitch = false;
    tasklist_t* tasklists;
    uint8_t numOfTaskLists;
    uint64_t procID = 0;

    void IdleTask(){
        while(1);//sys_task_sleep_cpp(1000);
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
        /*switch (this->blockType){
            case 1: //Sleep
                return (GetAPICTick() > this->block) ? true : false;
            case 2: //IO Wait
                io_wait(); //idk how to implement that yet
                return true;
            default:
                return true;
        }*/
       return (this->blockType == 1) ? (GetAPICTick() > this->block)
     : (this->blockType == 2) ? true // assuming IO always unblocks
     : true;
    }

    void task_t::setName(char* name){
        strcpy((char*) &this->task_name, name);
    }

    void task_t::initialize(void* function){
        this->function = (func_ptr)function;
        this->stack = (uint64_t)GlobalAllocator.RequestPages((4 * 1024 * 1024) / 0x1000);
        memset((void*)this->stack, 0, (4 * 1024 * 1024));
        this->kernelStack = (uint64_t)GlobalAllocator.RequestPages((4 * 1024 * 1024) / 0x1000);
        memset((void*)this->kernelStack, 0, (4 * 1024 * 1024));

        this->state = Stopped;
        this->resetCounter();
        this->setName("Unnamed Task");
        this->pid = procID;
        procID++;
    }

    /*
    +---------------------+  <-- rsp (stack pointer) on entry
    | argc (int)          |  <-- number of arguments
    +---------------------+
    | argv[0] (char *)    |  <-- pointer to first arg string
    +---------------------+
    | argv[1] (char *)    |
    +---------------------+
    | ...                 |
    +---------------------+
    | argv[argc] = NULL   |  <-- argv ends with NULL
    +---------------------+
    | envp[0] (char *)    |  <-- pointer to first env string
    +---------------------+
    | envp[1] (char *)    |
    +---------------------+
    | ...                 |
    +---------------------+
    | envp[n] = NULL      |  <-- envp ends with NULL
    +---------------------+
    | auxiliary vector    |  <-- series of Elf64_auxv_t entries (pairs)
    +---------------------+
    | NULL auxv entry     |  <-- auxv ends with AT_NULL entry
    +---------------------+
    | padding             |  <-- align to 16 bytes if needed
    +---------------------+
    | arg strings         |  <-- actual argument strings (argv strings)
    +---------------------+
    | env strings         |  <-- actual env variable strings
    +---------------------+
    */


    void setup_stack(task_t* task, int argc, char* argv[], char* envp[], auxv_t auxv_entries[]){
        uint8_t* stack_top = (uint8_t*)(task->stack + (4 * 1024 * 1024)); // top of stack
        uint8_t* sp = stack_top;
        uint8_t* auxv_addr = sp;
        PUSH_TO_STACK(sp, uint64_t, 0);
        PUSH_TO_STACK(sp, uint64_t, AT_NULL);
        for (int i = 0;; i++){
            auxv_t auxv = auxv_entries[i];
            if (auxv.a_type == AT_NULL) break;
            PUSH_TO_STACK(sp, uint64_t, auxv.a_val);
            PUSH_TO_STACK(sp, uint64_t, auxv.a_type); // MOVE 16 bytes per entry
            //kprintf("T: %d val: %llx\n", auxv.a_type, auxv.a_val);
        }

        //memcpy(auxv_addr, auxv_entries, bytes);

        PUSH_TO_STACK(sp, uint64_t, 0); // NULL ENVP

        for (int i = 0;; i++){
            char* env = envp[i];
            if (env == nullptr) break;
            int len = strlen(env);
            char* str = new char[len + 1];
            strcpy(str, env);
            PUSH_TO_STACK(sp, uint64_t, (uint64_t)str); // ENVP
        }
        task->rdx = (uint64_t)sp;

        PUSH_TO_STACK(sp, uint64_t, 0); // NULL argv
        for (int i = argc - 1; i >= 0; i--){
            char* arg = argv[i];
            int len = strlen(arg);
            char* str = new char[len + 1];
            strcpy(str, arg);
            PUSH_TO_STACK(sp, uint64_t, (uint64_t)str);
        }
        task->rsi = (uint64_t)sp;

        PUSH_TO_STACK(sp, uint64_t, argc); // ARGC
        task->rdi = argc;

        task->rsp = (uint64_t)sp;
    }

    task_t* CreateTask(void* function, uint8_t priority, bool user){
        if (priority > (numOfTaskLists - 1)) priority = numOfTaskLists - 1;
        task_t* task = new task_t;
        
        task->brk = BRK_DEFAULT_BASE;
        task->brk_end = BRK_DEFAULT_BASE;
        task->initialize(function);
        task->userspace = user;
        task->tasklist = priority;
        task->ptm = nullptr;
        task->tmp_PML4 = NULL;
        task->Context = nullptr;
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
            /*GlobalAllocator.FreePages((void*)task->stack, 10);
            GlobalAllocator.FreePages((void*)task->kernelStack, 10);
            if (task->ptm != nullptr){
                GlobalAllocator.FreePage(task->ptm->PML4);
                free(task->ptm);
            }*/
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
            for (int i = 0; i < list->numOfTasks; i++){
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
        task_t* task = nullptr;
        while (true){
            task = FindNextTask();
            if (!task){
                ResetAllCounters();
                continue;
            }
            if (task->exit){
                RemoveTask(task);
                continue;
            }
            break;
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

        next_syscall_stack = currentTask->kernelStack + (4 * 1024 * 1024);
        
        // We dont need to save / load the user stack, since no interrupts will be fired during a syscall

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
                asm volatile ("mov %0, %%rsp" :: "r" (currentTask->rsp ? currentTask->rsp : currentTask->stack + (4 * 1024 * 1024)));
                asm volatile ("mov %0, %%rdi" :: "r" (currentTask) : "rdi");
                
                currentTask->state = Running;
                currentTask->function(currentTask->Context);

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
        set_kernel_stack(nextTask->kernelStack + (4 * 1024 * 1024), cpu0_tss_entry);
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