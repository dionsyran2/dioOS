#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>

int listen(int sockfd, int backlog){
    kprintf("LISTENING\n");
    task_t *self = task_scheduler::get_current_task();

    // Get the socket
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADFD;

    if (sock->node->type != VSOC) return -ENOTSOCK;

    socket_info *info = (socket_info *)sock->node->file_identifier;
    if (!info) return -EFAULT;

    info->backlog = backlog;
    info->state = LISTENING;

    return 0;
}

REGISTER_SYSCALL(SYS_listen, listen);