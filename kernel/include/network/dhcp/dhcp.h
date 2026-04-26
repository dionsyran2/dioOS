#pragma once
#include <stdint.h>
#include <stddef.h>
#include <network/common/common.h>

#define DHCP_OP_REQ 1
#define DHCP_OP_RES 2

struct dhcp_packet_t {
    uint8_t  op;
    uint8_t  htype;         // 1 = Ethernet
    uint8_t  hlen;          // 6 = MAC length
    uint8_t  hops;          // 0
    uint32_t xid;           // Transaction ID (Random number you generate)
    uint16_t secs;          // 0
    uint16_t flags;         // 0x8000 for Broadcast
    uint32_t ciaddr;        // Client IP
    uint32_t yiaddr;        // Your IP
    uint32_t siaddr;        // Server IP
    uint32_t giaddr;        // Gateway IP
    uint8_t  chaddr[16];    // Client Hardware Address (Your MAC address)
    uint8_t  sname[64];     // Server Name (all zeros)
    uint8_t  file[128];     // Boot filename (all zeros)
    uint32_t magic_cookie;  // Magic number to prove this is DHCP
    uint8_t  options[312];  // Variable options
} __attribute__((packed));

namespace network{
    namespace dhcp{
        void send_discover(uint64_t nic_id);
        void renew(uint64_t id, bool broadcast = false);
        void rebind(uint64_t nic);
    }
}