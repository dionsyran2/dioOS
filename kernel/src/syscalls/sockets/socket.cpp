#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>


bool unix_sock_listen_pollout(vnode_t *socket){
    unix_socket_t *info = (unix_socket_t *)socket->file_identifier;
    if (!info) return false;

    return info->pending_list.size() != 0;
}

int sock_gen_close(vnode_t *socket){
    delete (socket_t*)socket->file_identifier;
    return 0;
}

int socket(int domain, int type, int protocol){
    task_t *self = task_scheduler::get_current_task();
    
    // Create the file descriptor
    vnode_t *sock = new vnode_t(VSOC);
    sock->open();

    socket_t *info = nullptr;

    switch (domain){
        case AF_UNIX:
            info = new unix_socket_t;
            info->domain = domain;
            info->type = type;
            info->protocol = protocol;
            info->file = sock;
            sock->file_operations.pollout = unix_sock_listen_pollout;
            sock->file_operations.close = sock_gen_close;
            break;

        case AF_INET:
            info = new inet_socket_t;
            info->domain = domain;
            info->type = type;
            info->protocol = protocol;
            info->file = sock;
            sock->file_operations.close = sock_gen_close;
            break;

        default:
            delete sock;
            return -EPROTONOSUPPORT;
    }

    sock->file_identifier = (uint64_t)info;
    return self->open_node(sock)->num;
}

REGISTER_SYSCALL(SYS_socket, socket);