#pragma once
#include <stdint.h>
#include <stddef.h>
#include <network/common/common.h>

struct udp_header_t {
    uint16_t src_port;  // Port you are sending from
    uint16_t dst_port;  // Port you are sending to
    uint16_t length;    // Length of UDP header + Data
    uint16_t checksum;  // Optional in IPv4! Set to 0.
} __attribute__((packed));

#define UDP_HEADER_SIZE sizeof(udp_header_t)

namespace network{
    namespace UDP{
        uint16_t allocate_port(uint64_t nic_id, uint16_t req, void (*cb)(void *buffer, size_t size, uint64_t nic_id));
        size_t write_header(int nic_id, void *buffer, uint32_t dest_ip, uint16_t source_port, uint16_t dst_port, uint16_t payload_size);
        void handle_packet(uint64_t nic, void *buffer, size_t size);
    }
}