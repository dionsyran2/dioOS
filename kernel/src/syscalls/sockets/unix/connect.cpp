#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>
#include <kstdio.h>

int unix_socket_t::connect(sockaddr *socket, uint64_t addrlen){
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
    

    // Resolve the path
    vnode_t *host = vfs::resolve_path(pathname);
    if (!host) return -ENOENT;
    if (host->type != VSOC) return -ENOTSOCK;

    unix_socket_t *hinfo = (unix_socket_t *)host->file_identifier;
    if (!hinfo) {
        return -EFAULT;
    }

    if (hinfo->state != LISTENING) return -ECONNREFUSED;

    // Create a pending connection object
    pending_connection *connection = new pending_connection();
    connection->task = self;
    connection->socket = this;

    // Add it to the list
    hinfo->pending_list.lock();
    hinfo->pending_list.add(connection);
    hinfo->pending_list.unlock();

    host->data_ready_to_read = true;
    host->close(); // We don't need the reference anymore

    // Unblock the server
    if (hinfo->sleeper) hinfo->sleeper->Unblock();

    self->Block(UNSPECIFIED, 0);
    if (self->woke_by_signal) {
        return -EINTR;
    }
    
    // Return the status value
    int ret = connection->status;
    delete connection;

    if (ret == 0){
        state = CONNECTED;
        outbound->pollin = true;
    }

    return ret;
}

