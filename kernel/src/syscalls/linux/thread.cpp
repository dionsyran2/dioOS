#include <syscalls/syscalls.h>
#include <syscalls/linux/thread.h>
#include <other/ELFLoader.h>
#include <cpu.h>
#include <random.h>

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

    bool found = false;
    for (int i = 0; i < num_of_children; i++){
        if (children[i]->state == ZOMBIE) continue;
        found = true;
        break; 
    }

    if (!found) return -ECHILD;


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
    

    return ret ? ret : -1;
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

extern "C" int sys_clone_child_entry(){
    asm ("cli");

    task_t* ctask = task_scheduler::get_current_task();

    __asm__ __volatile__("mov %0, %%rsp" :: "r" (ctask->rdx));
    __asm__ __volatile__("mov %0, %%rbp" :: "r" (ctask->rdi));

    return 0;
}

int sys_clone(unsigned long flags, void *stack, int *parent_tid, int *child_tid, unsigned long tls){
    /*uint64_t rsp = 0;
    uint64_t rbp = 0;
    __asm__ __volatile__("mov %%rsp, %0" : "=r" (rsp));
    __asm__ __volatile__("mov %%rbp, %0" : "=r" (rbp));

    task_t* ctask = task_scheduler::get_current_task();
    task_t* child = task_scheduler::clone(ctask, flags, (uint64_t)stack, tls, (function)sys_fork_child_entry);
    rsp = child->syscall_stack + (rsp - (ctask->syscall_stack));
    if (flags & CLONE_CHILD_CLEARTID) *child_tid = 0;
    if (flags & CLONE_PARENT_SETTID) *parent_tid = child->tid;
    if (flags & CLONE_CHILD_SETTID) *child_tid = child->tid;

    child->flags &= ~TASK_CPL3; // make sure the entry runs in cpl 0;
    child->rsp = (uint64_t)GlobalAllocator.RequestPage() + 0x1000;
    child->rdx = rsp;
    child->rdi = rbp;
    child->rsi = (uint64_t)stack;

    task_scheduler::mark_ready(child);

    ctask->counter = TASK_SCHEDULER_COUNTER_DEFAULT;
    return child->pid;*/

    uint64_t rsp = 0;
    uint64_t rbp = 0;
    __asm__ __volatile__("mov %%rsp, %0" : "=r" (rsp));
    __asm__ __volatile__("mov %%rbp, %0" : "=r" (rbp));

    task_t* ctask = task_scheduler::get_current_task();
    task_t* child = task_scheduler::fork_process(ctask, (function)sys_clone_child_entry);
    if (flags & CLONE_PARENT_SETTID) *parent_tid = child->tid;
    if (flags & CLONE_CHILD_SETTID) *child_tid = child->tid;

    child->flags &= ~TASK_CPL3; // make sure the entry runs in cpl 0;
    child->rsp = (uint64_t)GlobalAllocator.RequestPage() + 0x1000;
    child->rdx = rsp;
    child->rdi = rbp;
    child->current_user_stack_ptr = (uint64_t)stack != 0 ? (uint64_t)stack : user_syscall_stack;
    task_scheduler::mark_ready(child);
    ctask->counter = TASK_SCHEDULER_COUNTER_DEFAULT;
    return child->pid;
}

static const int FUTEX_HASHTABLE_SIZE = 256;

struct futex_waiter {
    task_t* task;
    futex_waiter* next;
    uint32_t bitset;
};

struct futex_queue {
    uint32_t* addr;
    futex_waiter* head;
    spinlock_t lock;
    futex_queue() : addr(nullptr), head(nullptr) { spin_unlock(&lock); }
};

// simple chained hash table
static futex_queue futex_table[FUTEX_HASHTABLE_SIZE];

static inline uint32_t futex_hash(uint32_t* uaddr) {
    return (((uintptr_t)uaddr) >> 2) % FUTEX_HASHTABLE_SIZE;
}

int sys_futex(uint32_t *uaddr, int futex_op, uint32_t val, const struct timespc *timeout, uint32_t *uaddr2, uint32_t val3) {
    int cmd = futex_op & FUTEX_CMD_MASK;
    bool is_private = futex_op & FUTEX_PRIVATE_FLAG;
    uint32_t h = futex_hash(uaddr);
    futex_queue& q = futex_table[h];
    bool use_realtime = futex_op & FUTEX_CLOCK_REALTIME;

    switch (cmd) {
    case FUTEX_WAIT: {
        uint32_t cur = *uaddr;
        if (cur != val) return -EAGAIN;

        spin_lock(&q.lock);
        futex_waiter waiter{ task_scheduler::get_current_task(), q.head };
        q.head = &waiter;
        spin_unlock(&q.lock);

        task_t* self = task_scheduler::get_current_task();
        if (timeout) {
            uint64_t deadline = APICticsSinceBoot +
                timeout->tv_sec * 1000 +
                timeout->tv_nsec / (1000000000 / 1000);
            task_scheduler::schedule_until(deadline, task_state_t::BLOCKED);
            spin_lock(&q.lock);
            futex_waiter** ptr = &q.head;
            while (*ptr) {
                if ((*ptr)->task == self) {
                    *ptr = (*ptr)->next;
                    break;
                }
                ptr = &(*ptr)->next;
            }
            spin_unlock(&q.lock);
            if (APICticsSinceBoot >= deadline) return -ETIMEDOUT;
        } else {
            task_scheduler::yield();
        }
        return 0;
    }

    case FUTEX_WAKE: {
        int woken = 0;
        spin_lock(&q.lock);
        while (q.head && woken < (int)val) {
            futex_waiter* w = q.head;
            q.head = w->next;
            task_scheduler::unblock(w->task);
            woken++;
        }
        spin_unlock(&q.lock);
        return woken;
    }

    case FUTEX_WAIT_BITSET: {
        if (*uaddr != val) return -EAGAIN;

        spin_lock(&q.lock);
        futex_waiter waiter{ task_scheduler::get_current_task(), q.head, val3 };
        q.head = &waiter;
        spin_unlock(&q.lock);

        task_t* self = task_scheduler::get_current_task();
        if (timeout) {
            uint64_t deadline = APICticsSinceBoot +
                timeout->tv_sec * 1000 +
                timeout->tv_nsec / (1000000000 / 1000);
            task_scheduler::schedule_until(deadline, task_state_t::BLOCKED);

            spin_lock(&q.lock);
            futex_waiter** ptr = &q.head;
            while (*ptr) {
                if ((*ptr)->task == self) {
                    *ptr = (*ptr)->next;
                    break;
                }
                ptr = &(*ptr)->next;
            }
            spin_unlock(&q.lock);

            if (APICticsSinceBoot >= deadline) return -ETIMEDOUT;
        } else {
            task_scheduler::yield();
        }
        return 0;
    }
    case FUTEX_WAKE_BITSET: {
        int woken = 0;
        spin_lock(&q.lock);
        futex_waiter** ptr = &q.head;
        while (*ptr && woken < (int)val) {
            if ((*ptr)->bitset & val3) {
                futex_waiter* w = *ptr;
                *ptr = (*ptr)->next;
                task_scheduler::unblock(w->task);
                woken++;
            } else {
                ptr = &(*ptr)->next;
            }
        }
        spin_unlock(&q.lock);
        return woken;
    }


    default:
        return -ENOSYS;
    }
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
    return 0;
}

int sys_tgkill(int tgid, int tid, int sig){
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
    return 0;
}

int sys_getpgrp(){
    task_t* ctask = task_scheduler::get_current_task();
    return ctask->pgid;
}

int sys_getrandom(uint8_t* buf, size_t buflen, unsigned int flags){
    for (int i = 0; i < buflen; i++){
        buf[i] = (uint8_t)(random() & 0xFF);
    }
    return buflen;
}

int sys_setresuid(int ruid, int euid, int suid){
    task_t* ctask = task_scheduler::get_current_task();
    ctask->uid = ruid;
    ctask->euid = euid;
    return 0;
}

int sys_setresgid(int rgid, int egid, int sgid){
    task_t* ctask = task_scheduler::get_current_task();
    ctask->gid = rgid;
    ctask->egid = egid;
    return 0;
}

int sys_getresuid(int* ruid, int* euid, int* suid){
    task_t* ctask = task_scheduler::get_current_task();
    *ruid = ctask->uid;
    *euid = ctask->euid;
    *suid = ctask->uid;
    return 0;
}

int sys_getresgid(int* rgid, int* egid, int* sgid){
    task_t* ctask = task_scheduler::get_current_task();
    *rgid = ctask->gid;
    *egid = ctask->egid;
    *sgid = ctask->gid;
    return 0;
}

int stub(){
    return 0; // :)
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

    register_syscall(SYSCALL_SETRESUID, (syscall_handler_t)sys_setresuid);
    register_syscall(SYSCALL_SETRESGID, (syscall_handler_t)sys_setresgid);
    register_syscall(SYSCALL_GETRESUID, (syscall_handler_t)sys_getresuid);
    register_syscall(SYSCALL_GETRESGID, (syscall_handler_t)sys_getresgid);

    register_syscall(SYSCALL_FORK, (syscall_handler_t)sys_fork);
    register_syscall(SYSCALL_WAIT4, (syscall_handler_t)sys_wait4);
    register_syscall(SYSCALL_EXECVE, (syscall_handler_t)sys_execve);
    register_syscall(SYSCALL_GETAFFINITY, (syscall_handler_t)sys_sched_getaffinity);
    register_syscall(SYSCALL_CLONE, (syscall_handler_t)sys_clone);
    register_syscall(SYSCALL_FUTEX, (syscall_handler_t)sys_futex);
    register_syscall(SYSCALL_TKILL, (syscall_handler_t)sys_tkill);
    register_syscall(SYSCALL_TGKILL, (syscall_handler_t)sys_tgkill);
    register_syscall(SYSCALL_GETPGRP, (syscall_handler_t)sys_getpgrp);
    register_syscall(SYSCALL_GETRANDOM, (syscall_handler_t)sys_getrandom);
    register_syscall(273, (syscall_handler_t)stub);

}