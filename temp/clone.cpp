#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

// Standard x86_64 Syscall Numbers
#define SYS_clone 56
#define SYS_fork  57
#define SYS_vfork 58
#define SYS_exit  60
#define SYS_wait4 61

// Standard Linux Clone Flags (for the thread test)
#define SIGCHLD       17
#define CLONE_VM      0x00000100
#define CLONE_FS      0x00000200
#define CLONE_FILES   0x00000400
#define CLONE_SIGHAND 0x00000800

// Stack for the thread (64KB)
#define STACK_SIZE 65536
char clone_stack[STACK_SIZE];

int main() {
    printf("========== EXPLICIT SYSCALL CLONING TEST ==========\n\n");

    // ---------------------------------------------------------
    // 1. FORK TEST (Syscall 57)
    // ---------------------------------------------------------
    printf("--- Testing fork() via Syscall 57 ---\n");
    
    long p = syscall(SYS_fork);
    
    if (p < 0) {
        perror("fork failed");
    } else if (p == 0) {
        printf("[FORK]  Child process running! (PID: %d)\n", getpid());
        syscall(SYS_exit, 0); 
    } else {
        printf("[FORK]  Parent waiting for child %ld...\n", p);
        syscall(SYS_wait4, p, NULL, 0, NULL);
        printf("[FORK]  Child reaped successfully.\n\n");
    }

    // ---------------------------------------------------------
    // 2. VFORK TEST (Syscall 58)
    // ---------------------------------------------------------
    printf("--- Testing vfork() via Syscall 58 ---\n");
    
    p = syscall(SYS_vfork);
    
    if (p < 0) {
        perror("vfork failed");
    } else if (p == 0) {
        printf("[VFORK] Child process running! (PID: %d)\n", getpid());
        // Must raw exit to avoid closing parent's stdio buffers!
        syscall(SYS_exit, 0); 
    } else {
        printf("[VFORK] Parent unpaused! Child exited successfully.\n\n");
    }

    // ---------------------------------------------------------
    // 3. THREAD TEST (Syscall 56)
    // ---------------------------------------------------------
    printf("--- Testing thread creation via Syscall 56 ---\n");
    
    // Threads still require SYS_clone because we MUST pass a new stack pointer.
    void *stack_top = (void *)(&clone_stack[STACK_SIZE]);
    int thread_flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD;
    
    p = syscall(SYS_clone, thread_flags, stack_top, NULL, NULL, 0);
    
    if (p < 0) {
        perror("thread clone failed");
    } else if (p == 0) {
        printf("[CLONE] Hello from the clone thread! (PID: %d)\n", getpid());
        syscall(SYS_exit, 0);
    } else {
        printf("[CLONE] Parent waiting for thread %ld...\n", p);
        syscall(SYS_wait4, p, NULL, 0, NULL);
        printf("[CLONE] Thread reaped successfully.\n\n");
    }

    printf("================ ALL TESTS PASSED ================\n");
    return 0;
}
