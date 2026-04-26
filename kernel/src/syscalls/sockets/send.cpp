#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>

int64_t sendto(int sockfd, const void *buf, size_t size, int flags, const struct sockaddr *dest_addr, uint64_t addrlen){
   task_t *self = task_scheduler::get_current_task();

    // Get the socket
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADFD;

    if (sock->node->type != VSOC) return -ENOTSOCK;
    
    socket_t *socket = (socket_t *)sock->node->file_identifier;
    if (!socket) return -EFAULT;

    sockaddr *addr = nullptr;
    if (dest_addr){
        addr = (sockaddr*)malloc(addrlen);
        self->read_memory(dest_addr, addr, addrlen);
    }
    int ret = socket->sendto(buf, size, flags, addr, addrlen);
    if (dest_addr){
        free(addr);
    }

    return ret;
}

REGISTER_SYSCALL(SYS_sendto, sendto);


int sys_sendmsg(int sockfd, msghdr *user_msg, int flags) {
    task_t *self = task_scheduler::get_current_task();
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADF;

    msghdr msg;
    self->read_memory(user_msg, &msg, sizeof(msghdr));

    if (msg.msg_iovlen == 0) return 0;

    uint64_t iovcnt = msg.msg_iovlen;
    if (iovcnt < 0 || iovcnt > 1024) return -EINVAL;

    size_t iov_size = iovcnt * sizeof(struct iovec);
    
    struct iovec* iov = (struct iovec*)malloc(iov_size);
    if (!iov) return -ENOMEM;

    if (!self->read_memory((void*)msg.msg_iov, iov, iov_size)) {
        free(iov);
        return -EFAULT;
    }

    int total_len = 0;

    for (int i = 0; i < iovcnt; i++){
        int res = sendto(sockfd, iov[i].iov_base, iov[i].iov_len, 0, nullptr, 0);
        
        if (res < 0) {
            if (total_len > 0) break;
            free(iov); 
            return res;
        }
        total_len += res;
        
        if ((size_t)res < iov[i].iov_len) break;
    }

    free(iov);

    return total_len;
}
REGISTER_SYSCALL(SYS_sendmsg, sys_sendmsg); // 46