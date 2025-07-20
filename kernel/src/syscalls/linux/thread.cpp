#include <syscalls/syscalls.h>
#include <syscalls/linux/thread.h>
#include <other/ELFLoader.h>
#include <cpu.h>

#define ARCH_SET_FS     0x1002
#define ARCH_GET_FS     0x1003
#define ARCH_SET_GS     0x1001
#define ARCH_GET_GS     0x1004

void sys_exit_group(int status){
    task_t* ctask = task_scheduler::get_current_task();

    ctask->exit(status);
}


int sys_getuid(void){
    task_t* ctask = task_scheduler::get_current_task();
    return ctask->uid;
}

int sys_getgid(void){
    task_t* ctask = task_scheduler::get_current_task();
    return ctask->gid;
}

int sys_geteuid(void){
    task_t* ctask = task_scheduler::get_current_task();

    return ctask->euid;
}

int sys_getegid(void){
    task_t* ctask = task_scheduler::get_current_task();
    return ctask->egid;
}

long sys_arch_prctl(int op, unsigned long addr){
    unsigned long *write = (unsigned long *)addr; // in case its a read operation
    task_t* task = task_scheduler::get_current_task();
    switch (op){
        case ARCH_SET_FS:
            task->fs_base = addr;
            write_msr(IA32_FS_BASE, addr);
            break;
        case ARCH_GET_FS:
            *write = read_msr(IA32_FS_BASE);
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

long sys_set_tid_addr(unsigned long addr){
    task_t* ctask = task_scheduler::get_current_task(); 
    ctask->tid_address = addr;
    return ctask->pid;
}

int sys_getcwd(char* buf, size_t size){
    if (buf == nullptr) return -EFAULT;
    if (size == 0) return -EINVAL;

    task_t* task = task_scheduler::get_current_task();

    char* path = vfs::get_full_path_name(task->nd_cwd);
    if (strlen(path) >= size){
        delete[] path;
        return -ENAMETOOLONG;
    }
    
    strcpy(buf, path);
    delete[] path;
    return strlen(buf) + 1;
}

uint64_t sys_getpid(){
    task_t* task = task_scheduler::get_current_task();
    return task->pid;
}

uint64_t sys_getppid(){
    task_t* task = task_scheduler::get_current_task();
    return task->ppid < 0 ? task->pid : task->ppid;
}

int sys_getpgid(int pid){
    task_t* task = task_scheduler::get_process(pid);
    if (pid <= 0) task = task_scheduler::get_current_task();



    if (task == nullptr) return -ESRCH;
    return task->pgid;
}

int sys_setpgid(int pid, int pgid){
    task_t* task = task_scheduler::get_process(pid);
    if (pid <= 0) task = task_scheduler::get_current_task();


    if (task == nullptr) return -ESRCH;
    
    task->pgid = pgid;
    return task->pgid;
}

int sys_setgid(int gid){
    // should check for perms blah blah
    task_t* task = task_scheduler::get_current_task();
    task->egid = gid;
    task->gid = gid;
    return 0;
}

int sys_setuid(int uid){
    // should check for perms blah blah blah
    task_t* task = task_scheduler::get_current_task();
    task->euid = uid;
    task->uid = uid;
    return 0;
}

int sys_gettid(){
    task_t* task = task_scheduler::get_current_task();
    return task->tid;
}

extern "C" int sys_fork_child_entry(){
    asm ("cli");

    task_t* ctask = task_scheduler::get_current_task();
    user_syscall_stack = ctask->rsi;

    __asm__ __volatile__("mov %0, %%rsp" :: "r" (ctask->rdx));
    __asm__ __volatile__("mov %0, %%rbp" :: "r" (ctask->rdi));

    return 0;
}

int sys_fork(){
    uint64_t rsp = 0;
    uint64_t rbp = 0;
    __asm__ __volatile__("mov %%rsp, %0" : "=r" (rsp));
    __asm__ __volatile__("mov %%rbp, %0" : "=r" (rbp));

    task_t* ctask = task_scheduler::get_current_task();
    task_t* child = task_scheduler::fork_process(ctask, (function)sys_fork_child_entry);
    child->flags &= ~TASK_CPL3; // make sure the entry runs in cpl 0;
    child->rsp = (uint64_t)GlobalAllocator.RequestPage() + 0x1000;
    child->rdx = rsp;
    child->rdi = rbp;
    child->rsi = user_syscall_stack;
    task_scheduler::mark_ready(child);
    ctask->counter = TASK_SCHEDULER_COUNTER_DEFAULT;
    return child->pid;
}


int sys_wait4(int pid, int *wstatus, int options, struct rusage *rusage){
    task_t* ctask = task_scheduler::get_current_task();

    size_t num_of_children = 0;
    task_t** children = task_scheduler::get_children(ctask->pid, &num_of_children);
    if (num_of_children == 0) return -ECHILD;

    uint64_t user = user_syscall_stack;
    asm ("sti");
    if ((options & WNOHANG) == 0) task_scheduler::block_task(ctask, WAITING_ON_CHILD, pid);
    asm ("cli");

    user_syscall_stack = user;

    num_of_children = 0;
    int ret = 0;
    children = task_scheduler::get_children(ctask->pid, &num_of_children);
    for (size_t i = 0; i < num_of_children; i++){
        task_t* task = children[i];
        if (task->state == ZOMBIE){
            *wstatus = task->exit_code;
            ret = task->pid;
            task->state = task_state_t::DISABLED;
            task_scheduler::remove_task(task);
            break;
            // remove task
        }
    }
    

    return ret ? ret : -1; // since blocking fails, return an error. most apps will recall wait()
}

int sys_execve(char *pathname, char *const argv[], char *const envp[]){
    #ifdef LOG_SYSCALLS
    serialf("execve(%s, %llx, %llx)\n\r", pathname, argv, envp);
    #endif
    task_t* ctask = task_scheduler::get_current_task();

    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(ctask->nd_cwd);

        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
    }else if (pathname[0] != '/'){
        char* cwd = vfs::get_full_path_name(ctask->nd_cwd);

         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = new char[total_len];
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
    }

    vnode_t* node = vfs::resolve_path(pathname);
    if (node == nullptr) {
        return -ENOENT;
    }

    int argc = 0;
    while(true){
        if (argv[argc] == NULL) break;
        argc++;
    }

    char* argv_c[argc + 1];
    for (int i = 0; i < argc; i++){
        char* str = new char[strlen(argv[i]) + 1];
        strcpy(str, argv[i]);
        argv_c[i] = argv[i];
    }
    argv_c[argc] = NULL;



    int envc = 0;
    while(true){
        if (envp[envc] == NULL) break;
        envc++;
    }

    char* envp_c[envc + 1];
    for (int i = 0; i < envc; i++){
        char* str = new char[strlen(envp[i]) + 1];
        strcpy(str, envp[i]);
        envp_c[i] = envp[i];
    }

    envp_c[envc] = NULL;

    int ret = ELFLoader::kexecve(node, argc, argv_c, envp_c);
    
    return ret;
}

int sys_sched_getaffinity(unsigned int pid, size_t cpusetsize, unsigned long* mask) {
    if (cpusetsize < sizeof(unsigned long)) {
        return -EINVAL;
    }

    // Set bit 0 (CPU 0) as available
    *mask = 1;

    return 8;
}

int sys_clone(unsigned long flags, void *stack, int *parent_tid, int *child_tid, unsigned long tls){
    uint64_t rsp = 0;
    uint64_t rbp = 0;
    __asm__ __volatile__("mov %%rsp, %0" : "=r" (rsp));

    task_t* ctask = task_scheduler::get_current_task();
    task_t* child = task_scheduler::clone(ctask, flags, (uint64_t)stack, tls, (function)sys_fork_child_entry);
    rsp = child->syscall_stack + (rsp - (ctask->syscall_stack));

    if (flags & CLONE_CHILD_CLEARTID) *child_tid = 0;
    if (flags & CLONE_PARENT_SETTID) *parent_tid = child->tid;
    if (flags & CLONE_CHILD_SETTID) *child_tid = child->tid;

    child->flags &= ~TASK_CPL3; // make sure the entry runs in cpl 0;
    child->rsp = (uint64_t)GlobalAllocator.RequestPage() + 0x1000;
    child->rdx = rsp;
    child->rsi = (uint64_t)stack;

    task_scheduler::mark_ready(child);

    ctask->counter = TASK_SCHEDULER_COUNTER_DEFAULT;
    return child->pid;
}

int sys_futex(uint32_t *uaddr, int futex_op, uint32_t val, const struct timespc *timeout){
    uint64_t start_time = APICticsSinceBoot;
    asm ("sti");
    while(1){
        if (futex_op == FUTEX_WAIT){
            if (*uaddr != val) break;
        }else if (futex_op == 0x80){
            return 1;
        }else return -ENOSYS;

        if (timeout){
            if ((start_time + (timeout->tv_sec * 1000) + (timeout->tv_nsec / 1000000)) >= start_time) return -ETIMEDOUT;
        }
    }
    asm ("cli");
    return 0;
}

int sys_tkill(int tid, int sig){
    task_t* ctask = task_scheduler::get_current_task();
    task_t* target = nullptr;

    task_t* t = task_list;
    while (t != nullptr){
        if (t->pgid == ctask->pgid && t->tid == tid){
            target = t;
            break;
        }
        t = t->next;
    }

    if (target == nullptr) return -ESRCH;


    target->pending_signals_thread |= (1UL << sig);
    target->pending_signals_thread |= (1UL << sig);
    return 0;
}

void register_thread_syscalls(){
    register_syscall(SYSCALL_EXIT_GROUP, (syscall_handler_t)sys_exit_group);
    register_syscall(SYSCALL_GETUID, (syscall_handler_t)sys_getuid);
    register_syscall(SYSCALL_GETGID, (syscall_handler_t)sys_getgid);
    register_syscall(SYSCALL_GETEUID, (syscall_handler_t)sys_geteuid);
    register_syscall(SYSCALL_GETEGID, (syscall_handler_t)sys_getegid);
    register_syscall(SYSCALL_ARCH_PRCTL, (syscall_handler_t)sys_arch_prctl);
    register_syscall(SYSCALL_SET_TID_ADDR, (syscall_handler_t)sys_set_tid_addr);
    register_syscall(SYSCALL_GETCWD, (syscall_handler_t)sys_getcwd);
    register_syscall(SYSCALL_GETPID, (syscall_handler_t)sys_getpid);
    register_syscall(SYSCALL_GETPPID, (syscall_handler_t)sys_getppid);
    register_syscall(SYSCALL_GETPGID, (syscall_handler_t)sys_getpgid);
    register_syscall(SYSCALL_SETPGID, (syscall_handler_t)sys_setpgid);

    register_syscall(SYSCALL_SETGID, (syscall_handler_t)sys_setgid);
    register_syscall(SYSCALL_SETUID, (syscall_handler_t)sys_setuid);
    register_syscall(SYSCALL_GETTID, (syscall_handler_t)sys_gettid);

    register_syscall(SYSCALL_FORK, (syscall_handler_t)sys_fork);
    register_syscall(SYSCALL_WAIT4, (syscall_handler_t)sys_wait4);
    register_syscall(SYSCALL_EXECVE, (syscall_handler_t)sys_execve);
    register_syscall(SYSCALL_GETAFFINITY, (syscall_handler_t)sys_sched_getaffinity);
    register_syscall(SYSCALL_CLONE, (syscall_handler_t)sys_clone);
    register_syscall(SYSCALL_FUTEX, (syscall_handler_t)sys_futex);
    register_syscall(SYSCALL_TKILL, (syscall_handler_t)sys_futex);
}