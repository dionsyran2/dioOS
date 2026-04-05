#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>

int sock_unlink(vnode_t* this_node){
    if (this_node->parent) this_node->parent->_remove_child(this_node);
    return 0;
}

int bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen){
    if (addrlen < sizeof(sockaddr)) return -EFAULT;

    task_t *self = task_scheduler::get_current_task();

    // Get the socket
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADFD;


    // Create a kernel structure for the address
    sockaddr *socket = (sockaddr *)malloc(addrlen + 1);
    if (!socket) return -ENOMEM;

    // Read it from the userspace
    self->read_memory((void*)addr, socket, addrlen);

    // Currently only unix sockets are supported
    if (socket->family != AF_LOCAL) return -EAFNOSUPPORT;

    // Read the unix socket address
    sockaddr_un *un = (sockaddr_un *)socket;
    un->sun_path[addrlen - sizeof(un->sun_family)] = '\0';

    // We don't support abstract sockets :) (Abstract paths start with a null byte)
    if (un->sun_path[0] == '\0'){
        //free(socket);
        //return -EINVAL;
        strcpy(un->sun_path, un->sun_path + 1);
    }

    char pathname[512];
    kfix_path(un->sun_path, pathname, sizeof(pathname), self);

    char* last_slash = strrchr(pathname, '/');
    char* parent_path;
    char* socket_name;

    if (last_slash == pathname) {
        parent_path = strdup("/");
        socket_name = strdup(pathname + 1);
    } else {
        size_t parent_len = last_slash - pathname;
        parent_path = (char*)malloc(parent_len + 1);
        memcpy(parent_path, pathname, parent_len);
        parent_path[parent_len] = '\0';
        
        socket_name = strdup(last_slash + 1);
    }
    
    
    vnode_t* parent_node = vfs::resolve_path(parent_path);
    if (!parent_node){
        parent_node = vfs::create_path(parent_path, VDIR);
    }
    free(parent_path);

    if (!parent_node) {
        free(socket_name);
        free(socket);
        return -ENOENT; 
    }

    if (parent_node->type != VDIR) {
        free(socket_name);
        free(socket);
        return -ENOTDIR;
    }

    // Open the node (so we don't break references) and add the socket to its children
    for (int i = 0; i < sock->node->ref_count; i++){
        parent_node->open();
    }

    strcpy(sock->node->name, socket_name); // Rename it

    free(socket_name); // Done with this

    sock->node->file_operations.unlink = sock_unlink;
    parent_node->_add_child(sock->node); // Add it to the directory
    
    return 0;
}

REGISTER_SYSCALL(SYS_bind, bind);