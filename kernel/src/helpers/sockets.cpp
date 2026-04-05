#include <helpers/sockets.h>
#include <syscalls/sockets.h>
#include <kerrno.h>

int socket_open(vnode_t* this_node){
    socket_info *info = (socket_info *)this_node->file_identifier;
    if (!info) return 0;

    if (info->outbound) info->outbound->open();
    if (info->inbound) info->inbound->open();
    return 0;
}

int socket_close(vnode_t* this_node){
    socket_info *info = (socket_info *)this_node->file_identifier;
    if (!info) return 0;

    if (info->outbound) info->outbound->close();
    if (info->inbound) info->inbound->close();
    return 0;
}

// @brief Reads length bytes from offset offset
int socket_read(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    socket_info *info = (socket_info *)this_node->file_identifier;
    if (!info) return -EFAULT;

    if (info->inbound) return info->inbound->read(offset, length, buffer);
    return -EOPNOTSUPP;
}

int socket_write(uint64_t offset, uint64_t length, const void* buffer, vnode_t* this_node){
    socket_info *info = (socket_info *)this_node->file_identifier;
    if (!info) return -EFAULT;

    if (info->outbound) return info->outbound->write(offset, length, buffer);
    return -EOPNOTSUPP;
}


int connect_socket_to_pipes(vnode_t *socket, vnode_t *inbound_pipe, vnode_t *outbound_pipe){
    socket_info *info = (socket_info *)socket->file_identifier;
    if (!info) return -EFAULT;

    info->inbound = inbound_pipe;
    info->outbound = outbound_pipe;

    socket->file_operations.open = socket_open;
    socket->file_operations.close = socket_close;
    socket->file_operations.read = socket_read;
    socket->file_operations.write = socket_write;

    return 0;
}