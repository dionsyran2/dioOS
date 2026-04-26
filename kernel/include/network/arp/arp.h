#pragma once
#include <stdint.h>
#include <stddef.h>
#include <network/common/common.h>


namespace network{
    namespace arp{
        void resolve_mac_address(uint64_t nic_id, uint32_t ipv4, uint8_t *out);
        void handle_packet(uint64_t nic, void *buffer, size_t size);
    }
}