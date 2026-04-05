#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>


int socket(int domain, int type, int protocol){
    task_t *self = task_scheduler::get_current_task();
    
    // Create the file descriptor
    vnode_t *sock = new vnode_t(VSOC);

    // Create the info struct
    socket_info *info = new socket_info();
    info->domain = domain;
    info->type = type;
    info->protocol = protocol;

    sock->file_identifier = (uint64_t)info;

    // Open it and return the file descriptor
    sock->open();
    return self->open_node(sock)->num;
}

REGISTER_SYSCALL(SYS_socket, socket);