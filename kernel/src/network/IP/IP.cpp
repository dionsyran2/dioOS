#include <network/IP/IP.h>
#include <kstdio.h>
#include <memory/heap.h>
#include <memory.h>
#include <syscalls/sockets.h>


namespace network{
    namespace IP{
        uint16_t _calculate_checksum(ipv4_header_t *header) {
            uint16_t *ptr = (uint16_t *)header;
            uint32_t sum = 0;
            
            size_t length = (header->version_ihl & 0x0F) * 4; 

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

        size_t write_header(uint64_t nic_id, void *buffer, uint8_t protocol, uint32_t source_ip, uint32_t destination_ip, uint16_t payload_size){
            uint8_t dest_mac[6];
            network::arp::resolve_mac_address(nic_id, destination_ip, dest_mac);

            uint8_t src_mac[6];
            nic_common::get_mac(nic_id, src_mac);

            size_t eth_size = network::ethernet::write_header(buffer, dest_mac, src_mac, htons(ETH_EHTERTYPE_IP));
            buffer = (void*)((uint64_t)buffer + eth_size);

            ipv4_header_t *header = (ipv4_header_t *)buffer;

            header->version_ihl = 0x45;
            header->dscp_ecn = 0;
            header->total_length = htons(sizeof(ipv4_header_t) + payload_size);
            header->ident = 0;
            header->flags_fragment = htons(0x4000);
            header->ttl = 128;
            header->protocol = protocol;
            header->src_ip = source_ip;
            header->dst_ip = destination_ip;

            header->checksum = 0;
            header->checksum = _calculate_checksum(header);

            return eth_size + sizeof(ipv4_header_t);
        }

        void handle_packet(uint64_t nic, void *buffer, size_t size){
            ipv4_header_t *header = (ipv4_header_t *)buffer;
            uint32_t our_ip = nic_common::get_ipv4(nic);
            uint32_t broadcast_ip = nic_common::get_broadcast_ip_address(nic);
            if (!broadcast_ip) broadcast_ip = 0xFFFFFFFF;

            if ((our_ip != header->dst_ip && our_ip != 0) && header->dst_ip != broadcast_ip) {
                serialf("PACKET REJECTED (IP)\n");

                return;
            }

            switch (header->protocol){
                case IP_PROTOCOL_UDP: 
                    network::UDP::handle_packet(nic, (void*)((uint64_t)buffer + sizeof(ipv4_header_t)), size - sizeof(ipv4_header_t));
                    break;
                case IP_PROTOCOL_ICMP:
                    nic_common::waiting_sockets.lock();
                    for (int i = 0; i < nic_common::waiting_sockets.size(); i++){
                        waiting_socket_t *ws = nic_common::waiting_sockets.get(i);

                        if (ws->address == header->src_ip && ws->socket->protocol == IPPROTO_ICMP){
                            inbound_packet packet;
                            packet.source_ip = header->src_ip;
                            packet.buffer = malloc(size);
                            packet.length = size;

                            memcpy(packet.buffer, buffer, size);

                            ws->socket->inbound_packets.add(packet);
                            if (ws->socket->sleeper) ws->socket->sleeper->Unblock();
                            ws->ref_cnt--;

                            if (ws->ref_cnt <= 0){
                                nic_common::waiting_sockets.remove(i);
                                i--;
                                delete ws;
                            }
                        }
                    }

                    nic_common::waiting_sockets.unlock();

                    network::icmp::handle_packet(nic, (void*)((uint64_t)buffer + sizeof(ipv4_header_t)), size - sizeof(ipv4_header_t), header->src_ip);
                    break;
            }
        }
    }
}