#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>
#include <syscalls/files/fcntl.h>

int sys_recvfrom(int sockfd, void *buf, size_t size, int flags, const struct sockaddr *source_addr, uint64_t addrlen){
    task_t *self = task_scheduler::get_current_task();

    // Get the socket
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADFD;

    if (sock->node->type != VSOC) return -ENOTSOCK;
    
    socket_t *socket = (socket_t *)sock->node->file_identifier;
    if (!socket) return -EFAULT;

    if (sock->flags & O_NONBLOCK){
        flags |= 0x40;
    }

    return socket->recvfrom(buf, size, flags, source_addr, addrlen);
}
REGISTER_SYSCALL(SYS_recfrom, sys_recvfrom);

int sys_recvmsg(int sockfd, msghdr *user_msg, int flags) {
    task_t *self = task_scheduler::get_current_task();
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADF;

    msghdr msg;
    self->read_memory(user_msg, &msg, sizeof(msghdr));

    if (msg.msg_iovlen == 0) return 0;

    uint64_t iovcnt = msg.msg_iovlen;
    if (iovcnt < 0 || iovcnt > 1024) return -EINVAL;
    if (iovcnt == 0) return 0;

    size_t iov_size = iovcnt * sizeof(struct iovec);
    struct iovec* iov = (struct iovec*)malloc(iov_size);
    if (!iov) return -ENOMEM;

    if (!self->read_memory((void*)msg.msg_iov, iov, iov_size)) {
        free(iov);
        return -EFAULT;
    }

    int total_len = 0;
    for (int i = 0; i < iovcnt; i++){
        char* base = (char*)iov[i].iov_base;
        size_t len = iov[i].iov_len;

        int current_flags = flags;

        if (total_len > 0) {
            current_flags |= 0x40;
        }
        
        struct sockaddr* name_ptr = (i == 0) ? (struct sockaddr*)msg.msg_name : nullptr;
        uint64_t name_len = (i == 0) ? msg.msg_namelen : 0;

        int res = sys_recvfrom(sockfd, base, len, current_flags, name_ptr, name_len);
        
        if (res < 0) {
            if (total_len > 0) break;
            free(iov);
            return res;
        }
        
        total_len += res;
        if ((size_t)res < len) break;
    }

    if (msg.msg_name) {
        msg.msg_namelen = sizeof(sockaddr_in); 
    } else {
        msg.msg_namelen = 0;
    }
    
    msg.msg_controllen = 0; 
    msg.msg_flags = 0;
    
    self->write_memory(user_msg, &msg, sizeof(msghdr));

    free(iov);
    return total_len;
}
REGISTER_SYSCALL(SYS_recvmsg, sys_recvmsg);