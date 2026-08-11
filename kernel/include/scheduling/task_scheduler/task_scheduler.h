#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cpu.h>

#include <paging/PageTableManager.h>
#include <scheduling/task_scheduler/cpu_task_queue.h>
#include <structures/trees/avl_tree.h>
#include <memory/vmm.h>
#include <scheduling/task_scheduler/file_descriptors.h>

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
    uint64_t fs_pointer;

    void *saved_fpu_state;

    /* Sync stuff */
    int* clear_child_tid;    // Pointer to userspace TID address

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


    /* METHODS */
    task_t(function entry, pid_t pid, tid_t tgid, bool is_userspace);
    
    void block();
    void block(uint64_t deadline, kstd::avl_tree_t<task_t*> *block_list);
    
    int read_from_userspace(void *kbuffer, const void *uaddress, size_t size);
    int write_to_userspace(void *uaddress, const void *kbuffer, size_t size);
    char *read_string(const char *uaddress);

    void unblock();

    void exit(int exit_code);
};

namespace task_scheduler {
    extern kstd::avl_tree_t<task_t *> timed_block_list;
    extern spinlock_t timed_block_list_lock;
    // methods
    task_t *get_current_task(); // It will return the current running task (self)
    
    task_t *create_process(const char *name, function entry, bool userspace, bool init = false);
    void mark_as_ready(task_t *task);
    void initialize_core();
    void scheduler_tick(__registers_t* regs, bool change_task = false);

    void swap_tasks();
}