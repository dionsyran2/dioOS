#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>
#include <kstdio.h>

vnode_t* unix_socket_t::accept(){
    task_t *self = task_scheduler::get_current_task();
    if (state != LISTENING) return nullptr;

    while (true){
        this->pending_list.lock();
        if (this->pending_list.size() > 0) break;

        this->sleeper = self;
        
        this->pending_list.unlock();

        self->Block(UNSPECIFIED, 0);
    }
    this->sleeper = nullptr;

    pending_connection *pending = this->pending_list.get(0);
    this->pending_list.remove(0);

    if (!pending) {
        this->pending_list.unlock();
        return nullptr;
    }

    if (this->pending_list.size() == 0) {
        file->data_ready_to_read = false;
    }

    this->pending_list.unlock();

    // Create a socket object to represent it
    unix_socket_t *host = new unix_socket_t;

    //host->ready_to_receive_data = true;

    // Create the info struct
    host->domain = this->domain;
    host->type = this->type;
    host->protocol = this->protocol;

    host->state = CONNECTED;

    pending->status = 0;


    // Open the socket and return its fd
    vnode_t *node = new vnode_t(VSOC);

    node->file_identifier = (uint64_t)host;
    host->file = node;
    node->ready_to_receive_data = true;
    
    // Create the pipes
    socket_transfer *t1 = new socket_transfer();
    socket_transfer *t2 = new socket_transfer();

    unix_socket_t *client = pending->socket;
    connect_socket_structs(host, client, t1, t2);
    host->outbound->pollin = true;


    node->open();

    pending->task->Unblock();
    return node;
}
