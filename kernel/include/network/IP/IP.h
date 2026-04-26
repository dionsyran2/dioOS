#pragma once
#include <stdint.h>
#include <stddef.h>
#include <network/common/common.h>


struct ipv4_header_t {
    uint8_t  version_ihl;
    uint8_t  dscp_ecn;
    uint16_t total_length;
    uint16_t ident;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed));

#define IPV4_HEADER_SIZE sizeof(ipv4_header_t)
#define IP_PROTOCOL_UDP  17
#define IP_PROTOCOL_ICMP 1

namespace network{
    namespace IP{
        size_t write_header(uint64_t nic_id, void *buffer, uint8_t protocol, uint32_t source_ip, uint32_t destination_ip, uint16_t payload_size);
        void handle_packet(uint64_t nic, void *buffer, size_t size);
    }
}