#pragma once
#include <stdint.h>
#include <stddef.h>
#include <network/common/common.h>

namespace network{
    namespace icmp{
        void handle_packet(uint64_t nic, void *buffer, size_t size, uint32_t source);
    }
}