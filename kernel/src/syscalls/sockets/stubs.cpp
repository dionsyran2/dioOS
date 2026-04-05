#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>

// 1. Syscall 51: getsockname
int sys_getsockname(int sockfd, sockaddr *addr, uint32_t *addrlen) {
    task_t *self = task_scheduler::get_current_task();
    if (!self->get_fd(sockfd)) return -EBADFD;

    // Just write AF_LOCAL (1) to trick the client
    uint16_t family = 1; 
    self->write_memory(addr, &family, sizeof(uint16_t));
    
    uint32_t len = 2; // sizeof(family)
    self->write_memory(addrlen, &len, sizeof(uint32_t));
    return 0;
}
REGISTER_SYSCALL(51, sys_getsockname); // 51

// 2. Syscall 52: getpeername
int sys_getpeername(int sockfd, sockaddr *addr, uint32_t *addrlen) {
    // For now, it's identical to getsockname. We just need to return success.
    return sys_getsockname(sockfd, addr, addrlen);
}
REGISTER_SYSCALL(52, sys_getpeername); // 52

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

struct msghdr {
    void *msg_name;
    uint32_t msg_namelen;
    iovec *msg_iov;
    uint64_t msg_iovlen;
    void *msg_control;
    uint64_t msg_controllen;
    int msg_flags;
};

int sys_recvmsg(int sockfd, msghdr *user_msg, int flags) {
    task_t *self = task_scheduler::get_current_task();
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADF;

    msghdr msg;
    self->read_memory(user_msg, &msg, sizeof(msghdr));

    if (msg.msg_iovlen == 0) return 0;

    // Read the first IO vector buffer
    iovec iov;
    self->read_memory(msg.msg_iov, &iov, sizeof(iovec));

    // Fall back to your standard socket READ logic here!
    // Since we are hacking it, just read directly into the user's buffer.
    // Example (replace with your actual socket buffer read function):
    int bytes_read = sock->node->read(sock->offset, iov.iov_len, iov.iov_base);
    
    if (bytes_read < 0) return bytes_read; // Pass the error (e.g., -EAGAIN)
    
    // Update offset (though sockets don't really use offsets)
    sock->offset += bytes_read;
    return bytes_read;
}
REGISTER_SYSCALL(47, sys_recvmsg); // 47

int sys_sendmsg(int sockfd, msghdr *user_msg, int flags) {
    task_t *self = task_scheduler::get_current_task();
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADF;

    msghdr msg;
    self->read_memory(user_msg, &msg, sizeof(msghdr));

    if (msg.msg_iovlen == 0) return 0;

    // Read the first IO vector buffer
    iovec iov;
    self->read_memory(msg.msg_iov, &iov, sizeof(iovec));

    // Fall back to your standard socket WRITE logic here!
    // Example:
    int bytes_written = sock->node->write(sock->offset, iov.iov_len, iov.iov_base);
    
    if (bytes_written < 0) return bytes_written;
    
    sock->offset += bytes_written;
    return bytes_written;
}
REGISTER_SYSCALL(46, sys_sendmsg); // 46