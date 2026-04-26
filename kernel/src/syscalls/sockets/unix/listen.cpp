#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>

int unix_socket_t::listen(){
    task_t *self = task_scheduler::get_current_task();

    state = LISTENING;

    return 0;
}

