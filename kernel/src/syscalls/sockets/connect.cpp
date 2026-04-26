#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>

int connect(int sockfd, sockaddr *addr, uint64_t addrlen){
    task_t *self = task_scheduler::get_current_task();

    // Get the socket
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADFD;

    if (sock->node->type != VSOC) return -ENOTSOCK;
    
    socket_t *socket = (socket_t *)sock->node->file_identifier;
    if (!socket) return -EFAULT;

    struct sockaddr *kaddr = (struct sockaddr *)malloc(addrlen);
    self->read_memory(addr, kaddr, addrlen);

    int ret = socket->connect(kaddr, addrlen);

    free(kaddr);
    return ret;
}

REGISTER_SYSCALL(SYS_connect, connect);