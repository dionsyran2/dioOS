#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>


bool inet_socket_t::pollin(){
    return true;
}

bool inet_socket_t::pollout(){
    return this->inbound_packets.size() != 0;
}