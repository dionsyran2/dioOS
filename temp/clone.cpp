#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    printf("========== GLIBC FORK() TEST ==========\n");
    printf("Parent process starting with PID: %d\n", getpid());

    // Call the standard glibc fork wrapper
    pid_t pid = fork();

    if (pid < 0) {
        // Error handling if fork fails (e.g., returns -ENOMEM)
        perror("fork() failed");
        return 1;
        
    } else if (pid == 0) {
        // ---------------------------------------------------
        // CHILD PROCESS BLOCK
        // ---------------------------------------------------
        printf("[Child]  Hello from the child! My PID is: %d\n", getpid());
        printf("[Child]  My parent's PID is: %d\n", getppid());
        printf("[Child]  Exiting with status 42...\n");
        
        // Use standard exit() to trigger your SYS_exit syscall
        exit(42); 
        
    } else {
        // ---------------------------------------------------
        // PARENT PROCESS BLOCK
        // ---------------------------------------------------
        printf("[Parent] Successfully spawned child with PID: %d\n", pid);
        printf("[Parent] Waiting for child to finish...\n");

        int status;
        // Wait for the specific child to exit (triggers your SYS_wait4 syscall)
        pid_t reaped_pid = waitpid(pid, &status, 0);

        if (reaped_pid == pid) {
            printf("[Parent] Child %d reaped successfully.\n", reaped_pid);
            
            // Extract the exit code using standard POSIX macros
            if (WIFEXITED(status)) {
                printf("[Parent] Child exited normally with status: %d\n", WEXITSTATUS(status));
            }
        } else {
            perror("[Parent] waitpid() failed");
        }
    }

    printf("========== TEST COMPLETE ==========\n");
    return 0;
}
