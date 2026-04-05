#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>


int connect(int sockfd, sockaddr *addr, uint64_t addrlen){
    task_t *self = task_scheduler::get_current_task();

    // Get the socket
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADFD;

    if (sock->node->type != VSOC) return -ENOTSOCK;


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

    if (un->sun_path[0] == '\0'){
        strcpy(un->sun_path, un->sun_path + 1);
    }
    
    // Resolve the path
    vnode_t *host = vfs::resolve_path(un->sun_path);
    if (!host) return -ENOENT;
    if (host->type != VSOC) return -ENOTSOCK;

    socket_info *hinfo = (socket_info *)host->file_identifier;
    if (!hinfo) {
        return -EFAULT;
    }

    serialf("%s %s %d\n", un->sun_path, host->name, hinfo->state);

    if (hinfo->state != LISTENING) return -ECONNREFUSED;

    // Create a pending connection object
    pending_connection *connection = new pending_connection();
    connection->task = self;
    connection->socket = sock->node;

    // Add it to the list
    uint64_t rflags = spin_lock(&hinfo->list_lock);
    connection->next = hinfo->pending_list;
    hinfo->pending_list = connection;

    // Increase the count
    hinfo->pending_count++;


    spin_unlock(&hinfo->list_lock, rflags);

    host->data_ready_to_read = true;
    host->close(); // We don't need the reference anymore

    // Unblock the server
    if (hinfo->waiting_for_connection) hinfo->task->Unblock();

    self->Block(UNSPECIFIED, 0);
    if (self->woke_by_signal) return -EINTR;
    
    // Return the status value
    int ret = connection->status;
    delete connection;

    if (ret == 0){
        ((socket_info *)sock->node->file_identifier)->state = CONNECTED;
    }

    return ret;
}

REGISTER_SYSCALL(SYS_connect, connect);