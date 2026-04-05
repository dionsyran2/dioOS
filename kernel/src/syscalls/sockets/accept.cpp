#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>


int accept(int sockfd, struct sockaddr *addr, uint64_t addrlen){
    task_t *self = task_scheduler::get_current_task();

    // Get the socket
    fd_t *sock = self->get_fd(sockfd);
    if (!sock) return -EBADFD;

    // Get the socket info
    socket_info *info = (socket_info *)sock->node->file_identifier;
    if (!info) return -EFAULT;
    if (info->state != LISTENING) return -EINVAL;

    // Block while there are no available tasks
    uint64_t rflags;
    while (true){
        rflags = spin_lock(&info->list_lock);
        if (info->pending_count > 0) break;

        info->task = self;
        info->waiting_for_connection = true;
        
        spin_unlock(&info->list_lock, rflags);
        self->ScheduleFor(1000, BLOCKED);
    }

    info->waiting_for_connection = false;

    pending_connection *pending = info->pending_list;
    if (!pending) {
        spin_unlock(&info->list_lock, rflags);
        return -EFAULT;
    }

    info->pending_list = pending->next;
    spin_unlock(&info->list_lock, rflags);

    // Create a socket object to represent it
    vnode_t *host = new vnode_t(VSOC);

    // Create the info struct
    socket_info *hinfo = new socket_info();
    hinfo->domain = info->domain;
    hinfo->type = info->type;
    hinfo->protocol = info->protocol;

    hinfo->state = HOST;

    host->file_identifier = (uint64_t)hinfo;

    host->open();

    // Create the pipes
    pipe_t *inbound_pipe = create_pipe();
    pipe_t *outbound_pipe = create_pipe();

    // Connect its read/write functions to the pipes
    connect_socket_to_pipes(host, inbound_pipe->read_node, outbound_pipe->write_node);

    // Update the connecting socket
    connect_socket_to_pipes(pending->socket, outbound_pipe->read_node, inbound_pipe->write_node);

    // Set the status
    pending->status = 0;

    // Wake up the task
    pending->task->Unblock();

    // Open the socket and return its fd
    return self->open_node(host)->num;
}

REGISTER_SYSCALL(SYS_accept, accept);