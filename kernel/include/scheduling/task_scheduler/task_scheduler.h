#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cpu.h>

#include <paging/PageTableManager.h>
#include <scheduling/task_scheduler/vm_tracker.h>
#include <filesystem/vfs/vfs.h>

#define TASK_NAME_SIZE 124
#define TASK_SCHED_DEFAULT_COUNT 1
#define TASK_STACK_SIZE (1 * 1024 * 1024) // 1 MB should be enough

#define BRK_DEFAULT_BASE 0x7FFF00000000
#define RMAP_DEFAULT_BASE 0x7FF000000000
#define KMAP_DEFAULT_BASE 0x7F0000000000 // Kernel Mapping Space (Where the kernel allocates the stack and other stuff)


typedef int pid_t;
typedef int tid_t;
typedef int sid_t;
typedef int gid_t;
typedef int uid_t;
typedef uint64_t __stack_t;

enum task_state_t{
    STOPPED = 0, /* Probably not run yet */
    RUNNING = 1, /* Running right now */
    PAUSED  = 3, /* Paused due to a context switch */
    BLOCKED = 4, /* Blocked (See block types below) */
    ZOMBIE = 5   /* The task has exited, memory freed. 
                    Waiting for the parent to read the status code */
};

enum task_block_type_t{
    UNSPECIFIED = 0,
    /* Wait until a vnode is available */
    WAIT_ON_VNODE_READ =    1,
    WAIT_ON_VNODE_WRITE =   2,
    WAIT_ON_VNODE_GENERIC = 3,

    /* Sleep */
    SLEEP = 4,

    /* Other Task */
    WAIT_FOR_CHILD = 5,

    /* MEMORY */
    MEMORY_NON_ZERO = 6,

    /* EVENTS */
    WAITING_ON_EVENT = 7,

    WAITING_ON_FUTEX = 8
};

struct task_t;

struct fd_t{
    vnode_t* node;
    int flags;
    int num;
    size_t offset;


    fd_t* next;
};

struct fd_list{
    int ref_cnt = 1;
    spinlock_t spinlock = 0;
    uint64_t rflags;

    vnode_t *cwd = nullptr;
    fd_t *file_descriptors = nullptr;

    void lock();
    void unlock();
};


typedef uint64_t kernel_sigset_t;
typedef void (*__sighandler_t)(int signal);

#define SA_NOCLDSTOP	0x00000001
#define SA_NOCLDWAIT	0x00000002
#define SA_SIGINFO	    0x00000004

#define SIG_DFL	((__sighandler_t)0)	/* default signal handling */
#define SIG_IGN	((__sighandler_t)1)	/* ignore signal */
#define SIG_ERR	((__sighandler_t)-1)	/* error return from signal */

union sigval {
	int sival_int;
	void *sival_ptr;
};

typedef struct {
	int si_signo;
	int si_code;
	union sigval si_value;
	int si_errno;
	pid_t si_pid;
	uid_t si_uid;
	void *si_addr;
	int si_status;
	int si_band;
} siginfo_t;


struct kernel_sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, void*, void*);
    };
    uint64_t sa_flags;         // Must be 8 bytes (unsigned long in kernel)
    void (*sa_restorer)(void); // The trampoline address
    uint64_t sa_mask;          // The mask is ALWAYS at the end in the kernel!
};


struct vm_tracker_t;
struct PageTableManager;

struct task_t{
    task_t* next;

    char name[TASK_NAME_SIZE];
    char executable_name[TASK_NAME_SIZE];

    
    function entry;

    bool has_run; // if this is set, it will try to continue the task
    bool is_ready; // it will not run if this is not set
    bool is_krnl;
    bool userspace;

    // As far as i understand, tgid is the group's id and pid the unique thread id... on gettid it returns pid and on getpid it returns the tgid
    pid_t pid; // Process id
    pid_t pgid; // Process group id
    pid_t ppid; // Parent process id
    tid_t tgid; // Thread group id
    sid_t sid; // Session id

    uid_t ruid; // Real user id
    gid_t rgid; // Real group id
    uid_t euid; // Effective user id
    gid_t egid; // Effective group id
    uid_t suid; // Saved user id
    gid_t sgid; // Saved group id

    // Signals
    kernel_sigset_t pending_signals;
    kernel_sigset_t kernel_signals; // To identify which signals are sent by the kernel (for si_code)
    kernel_sigset_t signal_mask;
    kernel_sigset_t next_signal_mask;
    kernel_sigaction signal_actions[64];
    kernel_sigset_t saved_signal_mask;

    uint32_t alarm;
    uint64_t prev_alarm;
    bool reocurring_alarm;

    uint64_t itimer_next_expiration; 
    uint64_t itimer_interval;

    uint32_t* clear_child_tid;

    bool signal_parent_on_exit;
    
    int priority;
    int counter;

    uint64_t quantum_start;
    uint64_t cpu_time; // Utilization

    uint64_t affinity; // A bitmap
    uint64_t start_time; // In ticks
    
    // vmm tracker
    vm_tracker_t *vm_tracker; // Any allocation, should be included here

    // ptm
    PageTableManager* ptm;

    // State/Blocking
    task_state_t task_state;
    task_block_type_t block_type; // If blocked
    uint64_t block_context; // If applicable (e.g. a vnode, The child's pid (thread id))
    task_state_t previous_state; // The state to revert to

    // Timed Scheduling
    uint64_t schedule_until; // In bsp ticks

    int status_code;

    uint64_t fs_pointer;
    __registers_t registers; // The registers saved during a context switch
    __registers_t *syscall_registers; // Saved during a syscall
    bool executing_syscall; // Whether rip points to userspace or kernel code

    int signal_count;
    bool woke_by_signal;
    
    __stack_t kernel_stack_top; // kernel stack
    __stack_t user_stack_top; // the user stack
    __stack_t syscall_stack_top; // syscall stack

    __stack_t signal_stack_top; // signal stack (does not always exist, may be null)
    __stack_t current_user_stack; // Incase we change task during a syscall, preserve the user stack pointer
    uint64_t userspace_return_address; // Where to return execution after the syscall (used for clone/fork)
    
    void* saved_fpu_state;

    // Expose to Userspace (/proc)
    vnode_t* proc_vfs_dir;

    // File Descriptors
    fd_list *file_list;
    vnode_t *ctty;
    
    /* TASK HELPERS */
    /*void* AllocatePage(bool COW = true); // Allocate and mark in vm_tracker
    void* AllocatePages(size_t amount, bool COW = true); // Allocate and mark in vm_tracker
    void UnallocatePage(void* page); // Free and unmark from vm_tracker*/
    void ScheduleFor(int64_t ms, task_state_t state, task_block_type_t block_type = UNSPECIFIED, uint64_t block_context = 0);
    void Block(task_block_type_t block_type, uint64_t context);
    void Unblock();
    char* read_string(void* address, size_t max_length);
    bool read_memory(const void* address, void* buffer, size_t length);
    bool write_memory(void* address, const void* buffer, size_t length);
    void exit(int status, bool silent = false); // Silent will not wake up any tasks waiting (To be used when cloning for execve)

    fd_t* get_fd(int num, bool lock = true);
    fd_t* open_node(vnode_t* node, int num = 0);
    void close_fd(int num);
};

namespace task_scheduler
{   
    extern task_t* task_list;
    extern spinlock_t task_list_lock;

    
    void initialize_scheduler();
    
    task_t* get_current_task();
    task_t* create_process(const char* name, function entry, bool userspace, bool ptm, bool init = false);
    task_t* fork_process(task_t* process, bool share_vm = false, bool share_files = false);
    
    void exit_process(task_t* thread, int status);

    void mark_ready(task_t* task);
    void scheduler_tick(__registers_t* regs, bool change_task = false);

    void wake_blocked_tasks(task_block_type_t block_type);
    task_t* get_process(pid_t pid);
    task_t *get_thread(tid_t tgid, pid_t pid);
        
    task_t* clone_for_execve(task_t* task);

    // Internal
    void _swap_tasks();
    void _add_task_to_list(task_t* task);
    void _remove_task_from_list(task_t* task);
    void _wake_waiting_tasks(pid_t pid);
    void _handle_signals(task_t *task);

} // namespace task_scheduler

void _add_dynamic_task_virtual_files(task_t *task);
void _init_cpu_proc_fs();

// @brief Defined in task_scheduler.asm
extern "C" void _execute_task(__registers_t* regs);