#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>

bool unix_socket_t::pollin(){
    if (state == CONNECTED){
        return outbound->pollin;
    }

    return false;
}

bool unix_socket_t::pollout(){
    if (state == CONNECTED){
        return inbound->pollout;
    }
    return false;
}