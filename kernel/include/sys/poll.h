#pragma once

#define POLLIN      0x0001    // There is data to read
#define POLLPRI     0x0002    // There is urgent data to read
#define POLLOUT     0x0004    // Writing now will not block
#define POLLERR     0x0008    // Error condition
#define POLLHUP     0x0010    // Hung up
#define POLLNVAL    0x0020    // Invalid request (e.g., fd not open)

struct pollfd {
    int fd;         // File descriptor
    short events;   // Requested events mask
    short revents;  // Returned events mask
};