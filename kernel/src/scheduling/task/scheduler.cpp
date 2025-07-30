#include <scheduling/task/scheduler.h>
#include <syscalls/syscalls.h>
#include <kernel.h>
#include <userspace/userspace.h>

task_t* task_list;
task_t* idle_task;
task_t* current_task;

size_t current_pid = 0;
size_t current_tid = 100000;
void* scheduler_stack = nullptr;

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
    uint8_t* stack_top = (uint8_t*)(task->stack + TASK_SCHEDULER_DEFAULT_STACK_SIZE); // top of stack
    uint8_t* sp = stack_top;
    uint8_t* auxv_addr = sp;

    // they start at 1 to account for the null entry
    int num_of_env = 1;
    for (int i = 0; envp[i] != nullptr; i++) num_of_env++;
    int num_of_auxv = 1;
    for (int i = 0; auxv_entries[i].a_type != AT_NULL; i++) num_of_auxv++;

    // Add padding to ensure 16 byte alignment

    int total_size = (num_of_auxv * 16) + (num_of_env * 8) + ((argc + 1) * 8) + 8;
    //serialf("Aligned to 0x10: %s %d\n\r", (total_size & 0xF) == 0 ? "Yes" : "No", total_size);
    if ((total_size & 0xF) != 0) PUSH_TO_STACK(sp, uint64_t, 0); 

    
    PUSH_TO_STACK(sp, uint64_t, 0);
    PUSH_TO_STACK(sp, uint64_t, AT_NULL);
    for (int i = num_of_auxv - 1; i >= 0; i--){
        auxv_t auxv = auxv_entries[i];
        PUSH_TO_STACK(sp, uint64_t, auxv.a_val);
        PUSH_TO_STACK(sp, uint64_t, auxv.a_type); // MOVE 16 bytes per entry
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


int get_available_fd(task_t* task){
    for (int i = 3; i < 100; i++){
        bool found = false;
        open_fd_t* fd = task->open_fds;
        while(fd != nullptr){
            if (fd->fd == (uint64_t)i){
                found = true;
                break;
            }
            fd = fd->next;
        }

        if (!found) return i;
    }
    return -1;
}


open_fd_t* task_t::open_node(vnode_t* node, int id){
    if (node == nullptr) return nullptr;
    node->open();
    
    int desc_id = id;
    if (id < 0) desc_id = get_available_fd(this);

    (*open_file_descriptors) += 1;


    open_fd_t* fd = new open_fd_t;
    memset(fd, 0, sizeof(open_fd_t));
    fd->node = node;
    fd->owner = this;
    fd->fd = desc_id;

    if (open_fds == nullptr){
        open_fds = fd;
        return fd;
    }

    open_fd_t* cfd = open_fds;
    while(1){
        if (cfd->next == nullptr){
            cfd->next = fd;
            fd->previous = cfd;
            break;
        }
        cfd = cfd->next;
    }
    
    return fd;
}

void task_t::close_fd(open_fd_t* fd){
    if (fd == nullptr) return;

    if (fd == open_fds){
        task_t* nt = task_list;
        while(nt != nullptr){
            if (nt->pid == pid){
                if (nt->open_fds == open_fds){
                    nt->open_fds = fd->next;
                }
            }
            nt = nt->next;
        }
    }

    if (fd->previous) fd->previous->next = fd->next;
    if (fd->next) fd->next->previous = fd->previous;

    fd->node->close();
    delete fd;
}

open_fd_t* task_t::get_fd(uint64_t fd){
    open_fd_t* ofd = open_fds;
    while(ofd != nullptr){
        if (ofd->fd == fd){
            break;
        }
        ofd = ofd->next;
    }

    return ofd;
}



void task_t::exit(int code){
    task_scheduler::disable_scheduling = true;
    // free any allocated memory + taskmgr internals.
    
    this->exit_code = code;
    this->state = task_state_t::ZOMBIE;
    serialf("EXIT %d pid: %d\n\r", code, pid);

    if (flags & SYSTEM_TASK && code != 0){
        task_scheduler::disable_scheduling = true;
        asm ("sti");
        Restart(); // a system task exited with an error code
    }


    if (ppid != -1){
        task_t* task = task_list;
        while (task != nullptr){
            if (task->pid == ppid && task->block == WAITING_ON_CHILD && (task->block_info == -1 || task->block_info == pid)){
                serialf("WAKING UP %d\n", task->pid);
                task_scheduler::unblock(task);
            }
            task = task->next;
        }
    }

    task_scheduler::disable_scheduling = false;
    while(1){
        asm ("sti");
        asm ("pause");
    }
}

namespace task_scheduler{
    bool disable_scheduling = true;
    task_t* next_task = nullptr;

    void idle(){
        while(1) {
            asm ("sti");
            __asm__("pause");
        }
    }

    void change_task(task_t* task){
        next_task = task;
        task_t* ctask = task_scheduler::get_current_task();
        ctask->counter = 0;
        asm ("int $0x23");
    }
    uint64_t allocate_stack(){
        return (uint64_t)GlobalAllocator.RequestPages(TASK_SCHEDULER_DEFAULT_STACK_SIZE_IN_PAGES);
    }

    void free_stack(void* pages){
        GlobalAllocator.FreePages(pages, TASK_SCHEDULER_DEFAULT_STACK_SIZE_IN_PAGES);
    }

    void block_task(task_t* task, block_type type, int data){
        //serialf("BLOCKING %d\n\r", data);
        task->block = type;
        task->block_info = data;
        task->state = BLOCKED;
        task->counter = 0;
        __asm__ ("int $0x23"); // trigger an apic timer interrupt, to make the scheduler swap tasks
    }
    
    void unblock_task(task_t* task){
        task->state = PAUSED;
        task->counter = TASK_SCHEDULER_COUNTER_DEFAULT;
    }

    void init(){
        scheduler_stack = (void*)(allocate_stack() + TASK_SCHEDULER_DEFAULT_STACK_SIZE);
        idle_task = create_process("IDLE", idle);
        mark_ready(idle_task);
    }

    task_t* get_current_task(){
        return current_task;
    }

    void add_task(task_t* task){
        if (task_list == nullptr){
            task_list = task;
            return;
        }

        task_t* ct = task_list;
        while(1){
            if (ct->next == nullptr){
                ct->next = task;
                task->previous = ct;
                break;
            }
            
            ct = ct->next;
        }
    }

    void remove_task(task_t* task){
        if (task_list == task){
            task_list = task->next;
            if (task->next) task->next->previous = nullptr;
            return;
        }

        if (task->previous) task->previous->next = task->next;
        if (task->next) task->next->previous = task->previous;

        // free any allocated memory blah blah blah
    }

    task_t* create_process(
        const char* name, function entry, task_t* parent, bool ptm, vnode_t* tty,
        char* envp, int uid, int gid, int euid, int egid, uint64_t rdi,
        uint64_t rsi, uint64_t rdx, uint64_t flags, vnode_t* cwd, vnode_t* root,
        session::session_t* session
    ){
        task_t* task = new task_t;
        memset(task, 0, sizeof(task_t));

        task->name = name;
        
        task->entry = entry;

        task->pid = current_pid;
        current_pid++;

        if (parent != nullptr){
            task->ppid = parent->pid;
        }else {
            task->ppid = task->pid;
        }

        task->tid = task->pid;
        task->pgid = task->pid;
        
        task->gid = gid;
        task->uid = uid;
        task->egid = egid == -1 ? gid : egid;
        task->euid = euid == -1 ? uid : euid;

        task->kstack = allocate_stack();
        task->syscall_stack = allocate_stack();
        task->stack = allocate_stack();
        task->sig_stack = allocate_stack();


        task->brk = BRK_DEFAULT_BASE;
        task->brk_end = BRK_DEFAULT_BASE;

        task->rmap_ptr = new uint64_t(RMAP_DEFAULT_BASE);

        task->rdi = rdi;
        task->rsi = rsi;
        task->rdx = rdx;

        task->flags = flags;

        task->tty = tty;

        int* newInt = new int(0);

        task->open_file_descriptors = newInt;



        if (cwd == nullptr) cwd = vfs::get_root_node();
        task->nd_cwd = cwd;

        if (root == nullptr) root = vfs::get_root_node();
        task->nd_root = root;

        task->counter = TASK_SCHEDULER_COUNTER_DEFAULT;

        if (ptm){
            PageTableManager* pt = new PageTableManager((PageTable*)GlobalAllocator.RequestPage());
            memset(pt->PML4, 0, 0x1000);
            pt->MapMemory(pt->PML4, pt->PML4);
            globalPTM.ClonePTM(pt);
            task->ptm = pt;
        }

        task->session = session;
        
        add_task(task);
        return task;
    }

    task_t* get_process(uint16_t pid){
        task_t* task = task_list;
        while(task != nullptr){
            if (task->pid == pid) return task; // return the main thread of that process
            task = task->next;
        }

        return nullptr;
    }

    void send_signal_to_group(int pid, uint64_t signal){
        serialf("SIG: %d PROC: %d\n\r", signal, pid);
        task_t* task = task_list;
        while(task != nullptr){
            if (task->pgid == pid){
                task->pending_signals |= (1UL << signal);
                break;
            }
            task = task->next;
        }
    }

    task_t* fork_process(task_t* process, function entry){
        if (process == nullptr) return nullptr; // :)

        task_t* task = new task_t;
        memset(task, 0, sizeof(task_t));

        task->name = process->name;
        
        task->entry = entry;

        task->pid = current_pid;
        current_pid++;

        task->tid_address = process->tid_address;

        task->ppid = process->pid;

        task->tid = task->pid;
        task->pgid = task->pid;
        
        task->gs_base = process->gs_base;
        task->fs_base = process->fs_base;

        task->gid = process->pid;
        task->uid = process->uid;
        task->egid = process->egid;
        task->euid = process->euid;

        task->kstack = allocate_stack();
        task->syscall_stack = allocate_stack();
        task->stack = allocate_stack();
        task->sig_stack = allocate_stack();

        task->brk = process->brk;
        task->brk_end = process->brk_end;

        task->rmap_ptr = process->rmap_ptr;

        task->tty = process->tty;

        int* newInt = new int(*process->open_file_descriptors);

        task->open_file_descriptors = newInt;

        task->nd_cwd = process->nd_cwd;

        task->nd_root = process->nd_root;

        task->counter = TASK_SCHEDULER_COUNTER_DEFAULT;

        PageTableManager* pt = new PageTableManager((PageTable*)GlobalAllocator.RequestPage());
        memset(pt->PML4, 0, 0x1000);
        process->ptm->ClonePTM(pt);
        task->ptm = pt;

        task->session = process->session;
        
        // clone the stack
        memcpy_simd((void*)task->stack, (void*)process->stack, TASK_SCHEDULER_DEFAULT_STACK_SIZE);
        memcpy_simd((void*)task->kstack, (void*)process->kstack, TASK_SCHEDULER_DEFAULT_STACK_SIZE);
        memcpy_simd((void*)task->syscall_stack, (void*)process->syscall_stack, TASK_SCHEDULER_DEFAULT_STACK_SIZE);

        for (int i = 0; i < TASK_SCHEDULER_DEFAULT_STACK_SIZE_IN_PAGES; i++){
            pt->MapMemory((void*)(process->stack + (i * 0x1000)), (void*)(task->stack + (i * 0x1000)));
            pt->MapMemory((void*)(process->kstack + (i * 0x1000)), (void*)(task->kstack + (i * 0x1000)));
            pt->MapMemory((void*)(process->syscall_stack + (i * 0x1000)), (void*)(task->syscall_stack + (i * 0x1000)));
        }

        task->stack = process->stack;
        task->kstack = process->kstack;
        task->syscall_stack = process->syscall_stack;



        // clone the ofds
        open_fd_t* fd = process->open_fds;
        while (fd != nullptr){
            open_fd_t* nfd = task->open_node(fd->node, fd->fd);
            nfd->flags = fd->flags;
            nfd->other_end = fd->other_end;

            fd = fd->next;
        }


        add_task(task);
        return task;
    }

    task_t* clone(task_t* process, unsigned long flags, unsigned long stack, unsigned long tls, function entry){
        if (process == nullptr) return nullptr; // :)
        task_t* task = new task_t;
        memset(task, 0, sizeof(task_t));

        task->name = process->name;
        
        task->entry = entry;

        task->pid = current_pid;
        current_pid++;
        
        task->ppid = (flags & CLONE_PARENT) ? process->ppid : process->pid;
        
        task->tid = task->pid;
        task->pgid = (flags & CLONE_THREAD) ? process->pgid : task->pid;
        
        task->gs_base = process->gs_base;
        task->fs_base = (flags & CLONE_SETTLS) ? tls : process->fs_base;

        task->gid = process->pid;
        task->uid = process->uid;
        task->egid = process->egid;
        task->euid = process->euid;

        task->kstack = allocate_stack();
        task->syscall_stack = allocate_stack();
        //task->stack = allocate_stack();
        task->stack = stack - TASK_SCHEDULER_DEFAULT_STACK_SIZE;
        task->sig_stack = allocate_stack();

        task->brk = process->brk;
        task->brk_end = process->brk_end;

        task->rmap_ptr = process->rmap_ptr;

        task->tty = process->tty;

        int* newInt = new int(*process->open_file_descriptors);

        task->open_file_descriptors = newInt;

        task->nd_cwd = (flags & CLONE_FS) ? process->nd_cwd : vfs::get_root_node();
        task->nd_root = (flags & CLONE_FS) ? process->nd_root : vfs::get_root_node();

        task->counter = TASK_SCHEDULER_COUNTER_DEFAULT;

        memcpy_simd((void*)task->kstack, (void*)process->kstack, TASK_SCHEDULER_DEFAULT_STACK_SIZE);
        memcpy_simd((void*)task->syscall_stack, (void*)process->syscall_stack, TASK_SCHEDULER_DEFAULT_STACK_SIZE);

        serialf("CloneVM: %d\n", flags & CLONE_VM);
        if (flags & CLONE_VM){
            task->ptm = process->ptm;
        }else{
            PageTableManager* pt = new PageTableManager((PageTable*)GlobalAllocator.RequestPage());
            memset(pt->PML4, 0, 0x1000);
            process->ptm->ClonePTM(pt);
            task->ptm = pt;

            for (int i = 0; i < TASK_SCHEDULER_DEFAULT_STACK_SIZE_IN_PAGES; i++){
                pt->MapMemory((void*)(process->kstack + (i * 0x1000)), (void*)(task->kstack + (i * 0x1000)));
                pt->MapMemory((void*)(process->syscall_stack + (i * 0x1000)), (void*)(task->syscall_stack + (i * 0x1000)));
            }

            task->kstack = process->kstack;
            task->syscall_stack = process->syscall_stack;
        }

        task->session = process->session;
        
        
        task->rsp = stack;
        serialf("CloneFiles: %d\n", flags & CLONE_FILES);

        if (flags & CLONE_FILES){
            task->open_fds = process->open_fds;
            task->open_file_descriptors = process->open_file_descriptors;
        }else{
            open_fd_t* fd = process->open_fds;
            while (fd != nullptr){
                open_fd_t* nfd = task->open_node(fd->node, fd->fd);
                nfd->flags = fd->flags;
                nfd->other_end = fd->other_end;

                fd = fd->next;
            }
        }

        add_task(task);
        return task;
    }

    task_t* create_thread(
        task_t* process, function entry, int uid, int gid, int euid, int egid, uint64_t rdi,
        uint64_t rsi, uint64_t rdx, uint64_t flags
    ){
        if (process == nullptr) return nullptr; // :)

        task_t* task = new task_t;
        memset(task, 0, sizeof(task_t));

        //task->name = process->name;
        
        task->entry = entry;

        task->ppid = process->pid;

        task->pid = process->pid;

        task->tid = current_tid;

        task->pgid = process->pgid; // process group id
        
        current_tid++;
        
        task->gid = gid;
        task->uid = uid;
        task->egid = egid == -1 ? gid : egid;
        task->euid = euid == -1 ? uid : euid;

        task->kstack = allocate_stack();
        task->stack = allocate_stack();
        task->syscall_stack = allocate_stack();
        task->sig_stack = allocate_stack();

        task->brk = BRK_DEFAULT_BASE;
        task->brk_end = BRK_DEFAULT_BASE;

        task->rmap_ptr = process->rmap_ptr;

        task->rdi = rdi;
        task->rsi = rsi;
        task->rdx = rdx;

        task->flags = flags;

        task->nd_cwd = process->nd_cwd;
        task->nd_root = process->nd_root;

        task->open_file_descriptors = process->open_file_descriptors;
        task->open_fds = process->open_fds;

        task->counter = TASK_SCHEDULER_COUNTER_DEFAULT;

        task->tty = process->tty;
        add_task(task);
        

        return task;
    }

    void mark_ready(task_t* task){
        task->state = task_state_t::STOPPED;
    }


    task_t* find_next_task(){
        if (next_task){
            next_task->counter = TASK_SCHEDULER_COUNTER_DEFAULT;
            task_t* t = next_task;
            next_task = nullptr;
            return t;
        }
        
        task_t* task = task_list;
        while(task != nullptr){
            if ((task->state != task_state_t::INTERRUPTED && task->state != task_state_t::DISABLED && task->state != task_state_t::BLOCKED && task->state != task_state_t::ZOMBIE) && task->counter > 0){
                return task;
            }

            task = task->next;
        }
        return nullptr;
    }

    int schedule_until(uint64_t ticks, task_state_t state){
        task_t* task = task_scheduler::get_current_task();
        if (task == nullptr) return -ESRCH;

        task->schedule_ticks = ticks;
        task->schedule_prev_state = task->state;
        task->schedule_until = true;
        task->state = state;
        task->counter = 0;
        task->block = BLOCK_UNKNOWN;
        asm ("sti");
        asm ("int $0x23");
        return 0;
    }

    int yield(){
        task_t* task = task_scheduler::get_current_task();
        if (task == nullptr) return -ESRCH;

        task->state = BLOCKED;
        task->block = YIELD;
        task->counter = 0;
        asm ("sti");
        asm ("int $0x23");
        return 0;
    }

    int unblock(task_t* task){
        if (task == nullptr) return -ESRCH;
        task->state = STOPPED;
        return 0;
    }

    void reset_counters(){
        task_t* task = task_list;
        while(task != nullptr){
            if (task->schedule_until && task->schedule_ticks >= APICticsSinceBoot){
                task->state = task->schedule_prev_state;
            }

            if ((task->state != task_state_t::INTERRUPTED && task->state != task_state_t::DISABLED && task->state != task_state_t::BLOCKED && task->state != task_state_t::ZOMBIE)){
                task->counter = TASK_SCHEDULER_COUNTER_DEFAULT;
            }
            task = task->next;
        }
    }

    task_t* get_next_task(){
        task_t* ret = find_next_task();
        if (ret == nullptr){
            reset_counters();
            ret = find_next_task();
        }

        if (ret == nullptr){
            idle_task->counter = TASK_SCHEDULER_COUNTER_DEFAULT;
            return idle_task;
        }

        return ret;
    }

    void _exit_process(int pid, int code){
        // rewrite this thing
        task_t* ctask = task_scheduler::get_current_task();

        task_t* task = task_list;
        bool marked_zombie = false;

        while(task != nullptr){
            if (task->pid == pid && task != ctask){
                task->state = (marked_zombie && !(ctask && ctask->pid == pid)) ? DISABLED : ZOMBIE;
                task->exit_code = code;
                if (marked_zombie) remove_task(task);
                if (!marked_zombie) marked_zombie = true;
            }
            task = task->next;
        }

        if (ctask && ctask->pid == pid){
            ctask->exit(code);
        }
    }
    void run_task(task_t* task);
    void _check_for_pending_signals(task_t* task){
        uint64_t pending_signals = task->pending_signals | task->pending_signals_thread;
        if (pending_signals == 0) return;
        if (task->executing_a_handler) return; // a handler is already executing... let it finish (it has interrupted some other thread)
        disable_scheduling = true;
        int signal = 0;
        while (pending_signals != 0){ // loop through all of the possible signals
            if ((pending_signals & 1) == 0) { pending_signals >>= 1; signal++; continue;}; // continue if signal is not pending
            if (signal == SIGCONT) {
                pending_signals >>= 1;
                signal++;
                task->state = STOPPED;
                continue;
            };

            // signal pending
            task_t* best_candidate = nullptr;
            
            // loop through all of the tasks,
            // if it is in the same process
            // and the flag is not set we set
            // it as the best candidate.
            // if it has a handler registered
            // we will use that one

            bool masked = false;

            if (task->pending_signals_thread & (1 << signal)){ // signal sent to this thread (tkill, tgkill, ...)
                best_candidate = task;
                task->pending_signals_thread &= ~(1UL << signal);
                task->pending_signals &= ~(1UL << signal);
                if (task->signal_mask & (1UL << signal)) masked = true; // signal is masked
            }else{ // signal is sent to the process, not a specific thread
                task_t* t = task_list;
                while(t != nullptr){
                    if (t->pid == task->pid && (t->signal_mask & (1 << signal)) == 0){
                        if (best_candidate != nullptr){
                            masked = false;
                            if (t->signals[signal].__sa_handler.sa_handler != nullptr) best_candidate = t;
                            if (t->signals[signal].__sa_handler.sa_handler != (void*)-1) break; // use this task if it has a registered handler
                        }else best_candidate = t;
                    }else{
                        if (t != nullptr && t->pid == task->pid) masked = true;
                    }
                    t = t->next;
                }
            }

            if ((!masked && best_candidate == nullptr) || best_candidate->signals[signal].__sa_handler.sa_handler == nullptr){ // if it is not masked and no handler is registered kill the process
                _exit_process(task->pid, signal);
            }else{
                if (best_candidate->signals[signal].__sa_handler.sa_handler == (void*)-1 || best_candidate->signals[signal].__sa_handler.sa_handler == (void*)1) { pending_signals >>= 1; task->pending_signals &= ~(1UL << signal); continue;}; // ignore
                // there is a registered handler
                serialf("best candidate: %s %llx %llx\n\r", best_candidate->name, best_candidate, best_candidate->signals[signal].__sa_handler.sa_handler);
                // make a new thread that runs the handler, with the same ids... mark it as a handler and disable the task.
                // if the task == the one currently resuming set the counter to 0 and spin loop
                // break to make that handler run.

                task_t* ntask = create_process("SIGNAL HANDLER", (function)best_candidate->signals[signal].__sa_handler.sa_handler);
                ntask->rdi = signal;
                ntask->stack = task->sig_stack;

                ntask->pid = best_candidate->pid;
                ntask->tid = best_candidate->tid;

                ntask->tid_address = best_candidate->tid_address;

                ntask->ppid = best_candidate->pid;
                ntask->pgid = best_candidate->pid;

                ntask->gs_base = best_candidate->gs_base;
                ntask->fs_base = best_candidate->fs_base;

                ntask->gid = best_candidate->pid;
                ntask->uid = best_candidate->uid;
                ntask->egid = best_candidate->egid;
                ntask->euid = best_candidate->euid;

                ntask->brk = best_candidate->brk;
                ntask->brk_end = best_candidate->brk_end;

                ntask->rmap_ptr = best_candidate->rmap_ptr;

                ntask->tty = best_candidate->tty;

                ntask->nd_cwd = best_candidate->nd_cwd;
                ntask->nd_root = best_candidate->nd_root;
                ntask->counter = TASK_SCHEDULER_COUNTER_DEFAULT;
                ntask->ptm = best_candidate->ptm;
                ntask->session = best_candidate->session;
                ntask->flags = TASK_CPL3;

                ntask->open_fds = best_candidate->open_fds;
                ntask->open_file_descriptors = best_candidate->open_file_descriptors;

                ntask->is_signal_handler = true;
                if (task->sig_sp) ntask->rsp = task->sig_sp;
                //add_task(ntask);
                mark_ready(ntask);

                best_candidate->state = INTERRUPTED;
                best_candidate->executing_a_handler = true;
                task->pending_signals &= ~(1UL << signal);
                disable_scheduling = false;
                asm ("sti");
                run_task(ntask);
                break;
            }

            pending_signals >>= 1;
            task->pending_signals &= ~(1UL << signal);
            signal++;
        }
        asm ("sti");
        disable_scheduling = false;
    }

    void run_task(task_t* task){
        current_task = task;
        _check_for_pending_signals(task);
        if (task->ptm != nullptr){
            asm volatile ("mov %0, %%cr3" :: "r" (task->ptm->PML4));
        }else{
            asm volatile ("mov %0, %%cr3" :: "r" (globalPTM.PML4));
        }

        write_msr(IA32_FS_BASE, task->fs_base);
        write_msr(IA32_GS_BASE, task->gs_base);

        next_syscall_stack = task->syscall_stack + TASK_SCHEDULER_DEFAULT_STACK_SIZE;
        user_syscall_stack = task->current_user_stack_ptr;
        set_kernel_stack(task->kstack + TASK_SCHEDULER_DEFAULT_STACK_SIZE, cpu0_tss_entry);
        if (task->started == false){ // brand new task
            task->started = true;
            task->state = task_state_t::RUNNING;
            if ((task->flags & TASK_CPL3) != 0){ // user mode
                RunTaskInUserMode(task);
            }else{
                asm volatile ("sti");
                asm volatile ("mov %0, %%rsp" :: "r" (task->rsp ? task->rsp : task->stack + TASK_SCHEDULER_DEFAULT_STACK_SIZE));
                asm volatile ("mov %0, %%rdi" :: "r" (task->rdi));
                asm volatile ("mov %0, %%rsi" :: "r" (task->rsi));
                asm volatile ("mov %0, %%rdx" :: "r" (task->rdx));
                
                if (!task->jmp) {
                    task->entry();
                }else{
                    asm ("jmp *%0" :: "r" (task->entry));
                }                
                asm volatile ("sti");
                current_task->exit(0);
                while(1);
            }
        } else{ // resume the task
            task->state = task_state_t::RUNNING;
            asm volatile ("mov %0, %%rsp" :: "r" (task->rsp));
            asm volatile ("mov %0, %%rbp" :: "r" (task->rbp)); //These are the stack values from the interrupt.

            asm volatile ("jmp *%0" :: "r" ((uint64_t)run_task_asm));
        }
    }

    void save_current_task(uint64_t rbp, uint64_t rsp){
        current_task->rbp = rbp;
        current_task->rsp = rsp;
        current_task->current_user_stack_ptr = user_syscall_stack;
    }

    void run_next_task(uint64_t rbp, uint64_t rsp){
        asm volatile ("mov %0, %%rsp" :: "r" (scheduler_stack));
        asm ("cli");
        task_t* next_task = get_next_task();

        if (current_task){
            save_current_task(rbp, rsp);
        }
        run_task(next_task);
    }


    void tick(uint64_t rbp, uint64_t rsp){
        if (disable_scheduling) return;

        if (current_task == nullptr) return;

        if (current_task->counter == 0) return run_next_task(rbp, rsp);

        current_task->counter--;
    }


    task_t** get_children(int parent, size_t* length){
        task_t** tasks = new task_t*[25];
        size_t off = 0;

        task_t* ctask = task_list;
        while(ctask != nullptr){
            if (ctask->ppid == parent && ctask->state != DISABLED && ctask->pid != parent){
                tasks[off] = ctask;
                off++;
            }
            ctask = ctask->next;
        }

        *length = off;
        return tasks;
    }

}