#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>


int sock_unlink(vnode_t* this_node){
    if (this_node->parent) this_node->parent->_remove_child(this_node);
    return 0;
}

int unix_socket_t::bind(const struct sockaddr *socket, uint32_t addrlen) { 
    if (addrlen < sizeof(sockaddr)) return -EFAULT;

    task_t *self = task_scheduler::get_current_task();

    if (socket->family != AF_LOCAL) {
        return -EAFNOSUPPORT;
    }

    // Read the unix socket address
    sockaddr_un *un = (sockaddr_un *)socket;

    // Abstract socket workaround
    if (un->sun_path[0] == '\0'){
        memcpy(un->sun_path, un->sun_path + 1, addrlen - sizeof(un->sun_family));
    }

    char original[addrlen - sizeof(un->sun_family) + 1] = { 0 };
    memcpy(original, un->sun_path, addrlen - sizeof(un->sun_family));
    char pathname[512];
    kfix_path(original, pathname, sizeof(pathname), self);

    vnode_t* existing_node = vfs::resolve_path(pathname);
    if (existing_node) {
        return -EADDRINUSE; // Address already in use
    }

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
        return -ENOENT; 
    }

    if (parent_node->type != VDIR) {
        free(socket_name);
        return -ENOTDIR;
    }

    // Open the parent directory node
    for (int i = 0; i < file->ref_count; i++){
        parent_node->open();
    }

    // Assign the name and add it to the VFS tree
    strcpy(file->name, socket_name);
    free(socket_name); 

    file->file_operations.unlink = sock_unlink;
    parent_node->_add_child(file); 
    
    return 0;
}
