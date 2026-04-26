#include <network/ethernet/ethernet.h>
#include <memory/heap.h>
#include <memory.h>
#include <kstdio.h>

namespace network{
    namespace ethernet{
        size_t write_header(void *buffer, uint8_t *destination_mac, uint8_t *source_mac, uint16_t ethertype){
            ethernet_header_t *header = (ethernet_header_t*)buffer;
            memcpy(header->dst_mac, destination_mac, 6);
            memcpy(header->src_mac, source_mac, 6);
            header->ethertype = ethertype;

            return sizeof(ethernet_header_t);
        }

        void handle_packet(uint64_t nic, void *buffer, size_t size){
            ethernet_header_t *header = (ethernet_header_t*)buffer;
            uint8_t mac[6];
            uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            nic_common::get_mac(nic, mac);

            if (memcmp(header->dst_mac, mac, 6) == 0 || memcmp(header->dst_mac, broadcast, 6) == 0){
                switch(ntohs(header->ethertype)){
                    case ETH_EHTERTYPE_IP:
                        network::IP::handle_packet(nic, (void*)((uint64_t)buffer + sizeof(ethernet_header_t)), size - sizeof(ethernet_header_t));
                        break;
                    case ETH_EHTERTYPE_ARP:
                        network::arp::handle_packet(nic, (void*)((uint64_t)buffer + sizeof(ethernet_header_t)), size - sizeof(ethernet_header_t));
                        break;
                    default:
                        serialf("[ETHERNET LAYER] UNKNOWN ETHERTYPE\n");
                }
            } else {
                serialf("PACKET REJECTED (ETH)\n");
            }

            free(buffer);
        }
    }
}