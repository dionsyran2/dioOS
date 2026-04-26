#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>

int bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen){
   task_t *self = task_scheduler::get_current_task();

    // Get the socket
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADFD;

    if (sock->node->type != VSOC) return -ENOTSOCK;
    
    socket_t *socket = (socket_t *)sock->node->file_identifier;
    if (!socket) return -EFAULT;

    struct sockaddr *kaddr = (struct sockaddr *)malloc(addrlen);
    self->read_memory(addr, kaddr, addrlen);

    int ret = socket->bind(kaddr, addrlen);

    free(kaddr);
    return ret;
}
REGISTER_SYSCALL(SYS_bind, bind);