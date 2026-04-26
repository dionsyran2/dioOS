#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>
#include <network/common/common.h>


int64_t inet_socket_t::sendto(const void *buf, size_t size, int flags, const struct sockaddr *dest_addr, uint64_t addrlen){
    task_t *self = task_scheduler::get_current_task();
    sockaddr_in* dest = (sockaddr_in*)dest_addr;
    uint32_t target_ip = dest->sin_addr.s_addr;
    
    uint64_t identifier = nic_common::get_nic_serving_ip(target_ip);
    if (identifier == -1) return -ENETDOWN;

    size_t buffer_size = IPV4_HEADER_SIZE + ETH_HEADER_SIZE + size;
    void *buffer = malloc(buffer_size);
    if (this->protocol != IPPROTO_ICMP) return -EPROTONOSUPPORT;

    uint8_t PROTO = IP_PROTOCOL_ICMP;
    size_t offset = network::IP::write_header(identifier, buffer, PROTO, nic_common::get_ipv4(identifier), target_ip, size);

    self->read_memory(buf, (uint8_t*)buffer + offset, size);

    nic_common::waiting_sockets.lock();

    bool found = false;

    for (int i = 0; i < nic_common::waiting_sockets.size(); i++){
        waiting_socket_t *ws = nic_common::waiting_sockets.get(i);

        if (ws->socket == this && ws->address == target_ip){
            found = true;
            ws->ref_cnt++;
            break;
        }
    }
    
    if (!found){
        waiting_socket_t *ws = new waiting_socket_t();
        ws->address = target_ip;
        ws->socket = this;
        
        nic_common::waiting_sockets.add(ws);
    }

    nic_common::waiting_sockets.unlock();
    nic_common::send(identifier, buffer, buffer_size);

    free(buffer);
    return size;
}
