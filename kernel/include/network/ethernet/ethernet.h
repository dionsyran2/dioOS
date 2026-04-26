#pragma once
#include <stdint.h>
#include <stddef.h>
#include <network/common/common.h>

#define ETH_HEADER_SIZE   14
#define ETH_EHTERTYPE_IP  0x0800
#define ETH_EHTERTYPE_ARP 0x0806

struct ethernet_header_t {
    uint8_t  dst_mac[6];  // Destination MAC Address
    uint8_t  src_mac[6];  // Source MAC Address
    uint16_t ethertype;   // Protocol Type (IPv4, ARP, IPv6, etc.)
} __attribute__((packed));


namespace network{
    namespace ethernet{
        void handle_packet(uint64_t nic, void *buffer, size_t size);
        size_t write_header(void *buffer, uint8_t *destination_mac, uint8_t *source_mac, uint16_t ethertype);
    }
}