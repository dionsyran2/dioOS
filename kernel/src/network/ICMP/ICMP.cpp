#include <network/ICMP/ICMP.h>
#include <memory/heap.h>
#include <memory.h>

struct icmp_header_t {
    uint8_t  type;       // 8 for Echo Request, 0 for Echo Reply
    uint8_t  code;       // Always 0 for pings
    uint16_t checksum;   // Same math as IPv4! (Set to 0 before calculating)
    uint16_t identifier; // Used to match replies to requests
    uint16_t sequence;   // A counter that goes up with each ping (1, 2, 3...)
} __attribute__((packed));

namespace network{
    namespace icmp{
        uint16_t _calculate_checksum(icmp_header_t *header, size_t length) {
            uint16_t *ptr = (uint16_t *)header;
            uint32_t sum = 0;
            
            while (length > 1) {
                sum += *ptr++;
                length -= 2;
            }

            if (length > 0) {
                sum += *(uint8_t *)ptr;
            }

            while (sum >> 16) {
                sum = (sum & 0xFFFF) + (sum >> 16);
            }

            return (uint16_t)(~sum);
        }

        void send_response(uint64_t nic, icmp_header_t *header, size_t size, uint32_t source);

        void handle_packet(uint64_t nic, void *buffer, size_t size, uint32_t source){

            icmp_header_t *packet = (icmp_header_t *)buffer;
            if (packet->type == 8) { // Echo Request
                send_response(nic, packet, size, source);
            }
        }

        void send_response(uint64_t nic, icmp_header_t *header, size_t size, uint32_t source){
            size_t buffer_size = size + IPV4_HEADER_SIZE + ETH_HEADER_SIZE;
            void *buffer = malloc(buffer_size);
            size_t offset = network::IP::write_header(nic, buffer, IP_PROTOCOL_ICMP, nic_common::get_ipv4(nic), source, size);
            icmp_header_t *packet = (icmp_header_t *)((uint64_t)buffer + offset);

            memcpy(packet, header, size);
            packet->type = 0; // Reply
            packet->checksum = 0;
            packet->checksum = _calculate_checksum(packet, size);

            nic_common::send(nic, buffer, buffer_size);
            free(buffer);
        }
    }
}