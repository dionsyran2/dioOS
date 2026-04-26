#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/helpers.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>

int getsockname(int sockfd, sockaddr *addr, uint32_t *addrlen) {
    return 0;
}
REGISTER_SYSCALL(SYS_getsockname, getsockname);