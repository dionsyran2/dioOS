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

    socket_info *info = (socket_info *)sock->node->file_identifier;
    if (!info) return -EFAULT;

    if (info->state == CONNECTED){
        // Its a connected socket, no network support yet, so we can freely use write
        return sys_write(sockfd, buf, size);
    } else {
        return -ENOTCONN;
    }

    return 0;
}

REGISTER_SYSCALL(SYS_sendto, sendto);