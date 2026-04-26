#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>

int listen(int sockfd, int backlog){
   task_t *self = task_scheduler::get_current_task();

    // Get the socket
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADFD;

    if (sock->node->type != VSOC) return -ENOTSOCK;
    
    socket_t *socket = (socket_t *)sock->node->file_identifier;
    if (!socket) return -EFAULT;

    return socket->listen();
}

REGISTER_SYSCALL(SYS_listen, listen);