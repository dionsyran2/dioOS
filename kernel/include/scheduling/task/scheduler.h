#pragma once
#include <stdint.h>
#include <stddef.h>
#include <session/session.h>
#include <scheduling/apic/apic.h>
#include <paging/PageFrameAllocator.h>
#include <paging/PageTableManager.h>
#include <VFS/vfs.h>
#include <syscalls/linux/signals.h>



#define TASK_SCHEDULER_COUNTER_DEFAULT              2              // The larger the quantum, the "laggier" the user experience - quanta above 100ms should be avoided.
#define TASK_SCHEDULER_DEFAULT_STACK_SIZE           4 * 1024 * 1024 // 4MB


#define TASK_SCHEDULER_DEFAULT_STACK_SIZE_IN_PAGES  \
    (TASK_SCHEDULER_DEFAULT_STACK_SIZE / 0x1000) + 1 // For 1MB this should be 256 pages


typedef struct {
    uint64_t a_type;
    uint64_t a_val;
} auxv_t;



#define BRK_DEFAULT_BASE 0x7F0000000000
#define RMAP_DEFAULT_BASE 0x7FF000000000
#define PUSH_TO_STACK(rsp, type, value)                                                 \
  rsp -= sizeof(type);                                                              \
  *((type *)(rsp)) = value



typedef void (*function)(void);

#define TASK_CPL3   (1 << 0)
#define SYSTEM_TASK (1 << 1)

typedef enum {
    DISABLED     = 0,
    PAUSED      = 1,
    BLOCKED     = 2,
    RUNNING     = 3,
    ZOMBIE      = 4,
    STOPPED     = 5,
    INTERRUPTED = 6
} task_state_t;

enum block_type{
    BLOCK_UNKNOWN = 0,
    WAITING_ON_CHILD = 1,
    YIELD = 2
};

struct open_fd_t;

typedef struct task {
    task* previous;
    task* next;

    const char* name;

    function entry;
    bool jmp;

    uint16_t pgid; // process group id
    int32_t ppid;
    uint16_t pid;
    uint16_t tid;

    uint16_t gid;
    uint16_t egid;
    uint16_t uid;
    uint16_t euid;

    uint64_t tid_address;
    int exit_code;

    uint64_t stack;
    uint64_t kstack;
    uint64_t syscall_stack;
    uint64_t sig_stack;
    uint64_t sig_sp;

    uint64_t brk;
    uint64_t brk_end;

    uint64_t* rmap_ptr;

    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rbp;
    uint64_t rsp;

    block_type block;
    int block_info;

    uint64_t counter;
    
    uint64_t flags;

    PageTableManager* ptm;

    int* open_file_descriptors;
    open_fd_t* open_fds; // its a chain, ending in null

    open_fd_t* fd_cwd;
    open_fd_t* fd_root;

    vnode_t* nd_cwd; // current working directory... if this is null, something has gone terribly wrong
    vnode_t* nd_root; // root directory... i am not exactly sure how this works... i will figure it out eventually
    
    vnode_t* tty;

    char* envp; // contains env entries, ends in a null pointer (envp[ν] = nullptr)
    task_state_t state;

    session::session_t* session;

    uint64_t tmp_data[8];

    uint64_t fs_base; // i totally didnt forget about this...
    uint64_t gs_base;

    uint64_t current_user_stack_ptr; // a place for the syscall handler to save the user rsp

    uint64_t pending_signals;
    uint64_t pending_signals_thread; // signals sent to this specific thread (tkill, tgkill, ...)
    uint64_t signal_mask;
    sigaction signals[54];

    bool is_signal_handler;
    bool executing_a_handler;

    bool schedule_until;
    uint64_t schedule_ticks;
    task_state_t schedule_prev_state;

    bool started;

    open_fd_t* open_node(vnode_t* node, int start = -1);
    void close_fd(open_fd_t* fd);
    open_fd_t* get_fd(uint64_t fd);
    void exit(int code);
} task_t;

struct open_fd_t {
    open_fd_t* next;
    open_fd_t* previous;
    uint64_t fd;
    vnode_t* node;
    int own;
    int own_type;
    char* data;
    task_t* owner;
    uint64_t offset;
    uint64_t length;
    uint64_t size_in_memory; //may be more than the length, usually aligned to a 4KB or 512 bytes
    uint64_t flags; // perms like r/w etc.
    uint64_t other_end; // pipes
    uint64_t dl_off;
};

namespace task_scheduler{
    extern bool disable_scheduling;
    extern size_t num_of_tasks;
    
    task_t* get_current_task();

    task_t* create_process(
        const char* name, function entry, task_t* parent = nullptr, bool ptm = false, vnode_t* tty = nullptr, char* envp = nullptr, int uid = -1, int gid = -1, int euid = -1, int egid = -1, uint64_t rdi = 0,
        uint64_t rsi = 0, uint64_t rdx = 0, uint64_t flags = 0, vnode_t* cwd = nullptr, vnode_t* root = nullptr, session::session_t* session = nullptr
    );

    task_t* create_thread(
        task_t* process, function entry, int uid = -1, int gid = -1, int euid = -1, int egid = -1, uint64_t rdi = 0,
        uint64_t rsi = 0, uint64_t rdx = 0, uint64_t flags = 0
    );

    task_t** get_children(int parent, size_t* length);

    void run_next_task(uint64_t rbp, uint64_t rsp);

    void mark_ready(task_t* task);

    void tick(uint64_t rbp, uint64_t rsp);

    task_t* get_process(uint16_t pid);

    task_t* fork_process(task_t* process, function entry);
    task_t* clone(task_t* process, unsigned long flags, unsigned long stack, unsigned long tls, function entry);

    void block_task(task_t* task, block_type type, int data);
    void unblock_task(task_t* task);
    void remove_task(task_t* task);
    void send_signal_to_group(int pid, uint64_t signal);
    
    uint64_t allocate_stack();
    void free_stack(void* pages);

    void init();

    int schedule_until(uint64_t tick, task_state_t block);

    int yield();

    int unblock(task_t* task);

    void change_task(task_t* task);
}

extern "C" void run_task_asm();
void setup_stack(task_t* task, int argc, char* argv[], char* envp[], auxv_t auxv_entries[]);
extern task_t* task_list;