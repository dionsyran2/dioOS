#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

// volatile sig_atomic_t guarantees safe read/writes between main and the handler
volatile sig_atomic_t signal_handled = 0;

void sigusr1_handler(int signum) {
    printf("[USER] Inside SIGUSR1 handler! (signum = %d)\n", signum);
    
    if (signum == SIGUSR1) {
        signal_handled = 1;
    }
    
    printf("[USER] Returning from handler...\n");
}

int main() {
    printf("[USER] Starting Signal Test Program...\n");

    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // libc will likely bitwise-OR SA_RESTORER into this under the hood

    // 1. Register the signal handler
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Failed to register sigaction");
        return 1;
    }
    printf("[USER] Registered SIGUSR1 handler successfully.\n");

    // 2. Get our own PID
    pid_t my_pid = getpid();
    printf("[USER] Sending SIGUSR1 to self (PID: %d)...\n", my_pid);

    // 3. Trigger the signal via your sys_kill implementation
    if (kill(my_pid, SIGUSR1) == -1) {
        perror("Failed to send signal");
        return 1;
    }

    // 4. If the kernel's stack hacking and sys_sigreturn work, 
    //    we should seamlessly land right here with registers perfectly intact.
    printf("[USER] Resumed execution in main()!\n");

    // 5. Verify the handler actually ran
    if (signal_handled) {
        printf("[USER] TEST PASSED: Signal was handled and state restored!\n");
        return 0;
    } else {
        printf("[USER] TEST FAILED: Signal was ignored or missed!\n");
        return 1;
    }
}
