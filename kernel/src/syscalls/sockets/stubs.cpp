#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>



// 3. Syscall 55: getsockopt (Specifically SO_PEERCRED)
int sys_getsockopt(int sockfd, int level, int optname, void *optval, uint32_t *optlen) {
    task_t *self = task_scheduler::get_current_task();
    if (!self->get_fd(sockfd)) return -EBADFD;
    
    // Level 1 = SOL_SOCKET, Opt 17 = SO_PEERCRED
    if (level == 1 && optname == 17) {
        struct ucred {
            int32_t pid;
            uint32_t uid;
            uint32_t gid;
        } cred;
        
        // Give Xorg the credentials of the root user / xinit
        cred.pid = self->pid; 
        cred.uid = 1000; // Assuming your user is 1000
        cred.gid = 1000;
        
        self->write_memory(optval, &cred, sizeof(cred));
        uint32_t len = sizeof(cred);
        self->write_memory(optlen, &len, sizeof(uint32_t));
        return 0;
    }
    
    return 0; // Return success for other random socket options
}
REGISTER_SYSCALL(55, sys_getsockopt); // 55


