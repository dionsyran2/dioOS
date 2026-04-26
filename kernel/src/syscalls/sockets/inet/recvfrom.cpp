#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>

int64_t inet_socket_t::recvfrom(void *buf, size_t size, int flags, const struct sockaddr *dest_addr, uint64_t addrlen){
    task_t *self = task_scheduler::get_current_task();
    bool non_blocking = (flags & 04000) || (flags & 0x40) || (type & 04000);

    while (!this->pollout()) {
        if (non_blocking) {
            return -EWOULDBLOCK;
        }
        
        int pri = self->priority;
        self->priority = 4;
        this->sleeper = self;
        self->Block(UNSPECIFIED, 0);
        this->sleeper = nullptr;
        self->priority = pri;
        
        // If the task was interrupted by a signal
        if (self->woke_by_signal) return -EINTR;
    }

    // We have a packet! Lock and pop EXACTLY ONE packet.
    this->inbound_packets.lock();
    inbound_packet packet = this->inbound_packets.get(0);
    this->inbound_packets.remove(0);
    this->inbound_packets.unlock();

    if (!packet.buffer) return -EFAULT;

    // Datagram short-read logic (Copy up to 'size' bytes max)
    int64_t bytes_to_copy = min(packet.length, size);

    if (!self->write_memory(buf, packet.buffer, bytes_to_copy)) {
        free(packet.buffer);
        return -EFAULT;
    }

    if (dest_addr && addrlen >= sizeof(sockaddr_in)) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = 0; // Raw sockets don't track ports
        addr.sin_addr.s_addr = packet.source_ip;
        memset(addr.sin_zero, 0, 8); // Padding
        
        self->write_memory((void*)dest_addr, &addr, sizeof(sockaddr_in));
    }

    free(packet.buffer);
    
    return bytes_to_copy; 
}