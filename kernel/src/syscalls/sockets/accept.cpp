#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>

#define LINUX_SOCK_NONBLOCK 04000 // 2048 in decimal

int accept(int sockfd, struct sockaddr *addr, uint64_t addrlen){
   task_t *self = task_scheduler::get_current_task();

    // Get the socket
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADFD;

    if (sock->node->type != VSOC) return -ENOTSOCK;
    
    socket_t *socket = (socket_t *)sock->node->file_identifier;
    if (!socket) return -EFAULT;

    vnode_t *node = socket->accept();
    if (!node) return -EFAULT;

    return self->open_node(node)->num;
}

REGISTER_SYSCALL(SYS_accept, accept);