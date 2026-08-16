#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cpu.h>

#include <paging/PageTableManager.h>
#include <scheduling/task_scheduler/cpu_task_queue.h>
#include <structures/trees/avl_tree.h>
#include <memory/vmm.h>
#include <scheduling/task_scheduler/file_descriptors.h>
#include <bits/signals.h>

#define CLONE_VM	0x00000100	/* set if VM shared between processes */
#define CLONE_FS	0x00000200	/* set if fs info shared between processes */
#define CLONE_FILES	0x00000400	/* set if open files shared between processes */
#define CLONE_SIGHAND	0x00000800	/* set if signal handlers and blocked signals shared */
#define CLONE_PIDFD	0x00001000	/* set if a pidfd should be placed in parent */
#define CLONE_PTRACE	0x00002000	/* set if we want to let tracing continue on the child too */
#define CLONE_VFORK	0x00004000	/* set if the parent wants the child to wake it up on mm_release */
#define CLONE_PARENT	0x00008000	/* set if we want to have the same parent as the cloner */
#define CLONE_THREAD	0x00010000	/* Same thread group? */
#define CLONE_NEWNS	0x00020000	/* New mount namespace group */
#define CLONE_SYSVSEM	0x00040000	/* share system V SEM_UNDO semantics */
#define CLONE_SETTLS	0x00080000	/* create a new TLS for the child */
#define CLONE_PARENT_SETTID	0x00100000	/* set the TID in the parent */
#define CLONE_CHILD_CLEARTID	0x00200000	/* clear the TID in the child */
#define CLONE_DETACHED		0x00400000	/* Unused, ignored */
#define CLONE_UNTRACED		0x00800000	/* set if the tracing process can't force CLONE_PTRACE on this clone */
#define CLONE_CHILD_SETTID	0x01000000	/* set the TID in the child */
#define CLONE_NEWCGROUP		0x02000000	/* New cgroup namespace */
#define CLONE_NEWUTS		0x04000000	/* New utsname namespace */
#define CLONE_NEWIPC		0x08000000	/* New ipc namespace */
#define CLONE_NEWUSER		0x10000000	/* New user namespace */
#define CLONE_NEWPID		0x20000000	/* New pid namespace */
#define CLONE_NEWNET		0x40000000	/* New network namespace */
#define CLONE_IO		0x80000000	/* Clone io context */


// Forward Declaration
struct mm_struct_t;
struct cpu_task_queue_t;

struct poll_table_t {
    void (*callback)(poll_table_t *pt);
    void *ctx;

    int events = 0;
    
    kstd::linked_list_t<kstd::linked_list_t<poll_table_t*>*> poll_list; // A list of poll lists (Where this one has been added)
};


#define DEFAULT_STACK_SIZE           (1 * 1024 * 1024) // 1 MB
#define DEFAULT_COUNTER_VALUE        1 // Default task counter (milliseconds in the current setup)
#define TASK_QUEUE_SIZE              256

typedef int pid_t;
typedef int tid_t;
typedef int sid_t;
typedef int gid_t;
typedef int uid_t;
typedef uint64_t __stack_t;

enum __task_state {
    NOT_STARTED, /* The task is brand new and has not been run yet */
    PAUSED, /* The task has been paused, due to a task switch */
    RUNNING, /* The task is currently being executed */
    BLOCKED, /* The task is blocked and part of the block queue */
    INTERRUPTABLE,
    ZOMBIE, /* Its dead (it has exited)*/
};


struct task_t {
    /* Task Information */
    char name[64];

    pid_t pid; // Process id
    tid_t tgid; // Thread group id

    pid_t ppid; // Parent Process Id

    sid_t sid = 0; // Session ID

    gid_t rgid = 0; // Real Group ID (The user group)
    uid_t ruid = 0; // Real User ID

    gid_t egid = 0; // Effective Group ID
    uid_t euid = 0; // Effective User ID

    gid_t sgid = 0; // Saved Group ID
    uid_t suid = 0; // Saved User ID

    bool is_userspace; // Whether this task should run on userspace

    /* STACKS (NOTE: These point to the top of the stacks!) */
    __stack_t userspace_stack; // The stack to be used for userspace (mapped at a specific address)
    __stack_t syscall_stack; // The stack to be used for syscalls


    __stack_t kernel_stack; // Used for interrupts when switching from user->kernel mode
                            // And for kernel-space tasks

    /* Memory */
    mm_struct_t *vmm = nullptr;

    /* Files */
    fd_table_t *fd_table;

    /* Execution stuff */
    function task_entry_point;

    __registers_t registers;
    __registers_t *syscall_registers; // A pointer to the user registers    
    bool is_executing_syscall = false;

    uint64_t fs_pointer;

    void *saved_fpu_state;

    /* Sync stuff */
    int* clear_child_tid;    // Pointer to userspace TID address

    /* Signals */
    uint64_t pending_signals = 0;
    uint64_t blocked_signals = 0;
    sigaction signal_actions[NSIG];
    
    /* Syscall Execution Stuff */
    uint64_t userspace_return_address; // Saves the return address of the syscall
    uint64_t current_user_stack; // Saves the return stack of the syscall

    /* Scheduling stuff */
    cpu_task_queue_t *cpu_queue; // The cpu queue this task is part of.
                               // (If its blocked it will not be in the queue but should be returned after unblocking)
    
    kstd::avl_tree_t<task_t *> *block_list; // Useful if time-blocked, so the scheduler can remove it from the list and prevent double-wakeups
    uint64_t block_deadline; // If time-blocked, for how much time?
    int block_status; // 0 on success, -ETIMEDOUT on timeout. (Reset to 0 on every block)
    uint64_t counter;
    uint64_t quantum_start;
    __task_state current_state;
    int exit_code;


    spinlock_t cleanup_lock;

    /* METHODS */
    task_t(function entry, pid_t pid, tid_t tgid, bool is_userspace);
    
    void block();
    void block(uint64_t deadline, kstd::avl_tree_t<task_t*> *block_list);
    
    int read_from_userspace(void *kbuffer, const void *uaddress, size_t size);
    int write_to_userspace(void *uaddress, const void *kbuffer, size_t size);
    char *read_string(const char *uaddress);
    
    void unblock();

    void exit(int exit_code);

    bool check_pending_signals();
    void deliver_signal(int signum);
    void restore_signal();
    void signal(int signum);
};

namespace task_scheduler {
    extern kstd::avl_tree_t<task_t *> timed_block_list;
    extern spinlock_t timed_block_list_lock;
    // methods
    task_t *get_current_task(); // It will return the current running task (self)
    
    task_t *search_by_pid(pid_t pid);
    task_t *create_process(const char *name, function entry, bool userspace, bool init = false);
    void mark_as_ready(task_t *task);
    void initialize_core();
    void scheduler_tick(__registers_t* regs, bool change_task = false);

    task_t *clone(uint64_t flags, uint64_t rsp, __registers_t *registers);

    void swap_tasks();
}