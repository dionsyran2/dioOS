#include <network/arp/arp.h>
#include <structures/lists/linked_list.h>
#include <drivers/timers/common.h>
#include <memory/heap.h>
#include <memory.h>
#include <kstdio.h>

#define ARP_CACHE_LIFETIME 150 // 2.5 mins

struct arp_packet_t {
    uint16_t hardware_type; // 1 for Ethernet
    uint16_t protocol_type; // 0x0800 for IPv4
    uint8_t  hardware_len;  // 6 (Length of a MAC address)
    uint8_t  protocol_len;  // 4 (Length of an IPv4 address)
    uint16_t opcode;        // 1 for Request, 2 for Reply
    uint8_t  sender_mac[6]; // Your MAC
    uint32_t sender_ip;     // Your IP
    uint8_t  target_mac[6]; // Their MAC (00:00:00:00:00:00 if asking)
    uint32_t target_ip;     // Their IP (The one you are looking for)
} __attribute__((packed));

struct arp_waiter{
    task_t *task;
    uint64_t nic_id;
    uint32_t ip;
};

kstd::linked_list_t<arp_waiter*> waiters;

namespace network{
    namespace arp{
        void send_arp_response(uint64_t nic, uint32_t ip, uint8_t* mac);

        void handle_packet(uint64_t nic, void *buffer, size_t size){
            arp_packet_t *packet = (arp_packet_t *)buffer;

            uint32_t our_ip = nic_common::get_ipv4(nic);
            if (packet->target_ip != our_ip) return;

            if (packet->opcode == htons(1) /* Request */ ) {
                if (packet->hardware_len != 6 || packet->protocol_len != 4) return; // Got no idea what it wants

                send_arp_response(nic, packet->sender_ip, packet->sender_mac);
            } if (packet->opcode == htons(2) /* Response */ ){
                kstd::linked_list_t<arp_cache*> *list = nic_common::get_arp_cache(nic);

                arp_cache *cache = new arp_cache;
                cache->ip_address = packet->sender_ip;
                memcpy(cache->mac_address, packet->sender_mac, 6);
                cache->timestamp = current_time;

                list->lock();
                list->add(cache);
                list->unlock();

                // Check the waiters and wake them up
                waiters.lock();
                for (int i = 0; i < waiters.size(); i++){
                    arp_waiter* waiter = waiters.get(i);

                    if (waiter->nic_id == nic && waiter->ip == packet->sender_ip){
                        waiters.remove(i);

                        if (waiter->task) waiter->task->Unblock();

                        i--;
                        break;
                    }
                }
                waiters.unlock();
            }
        }

        void send_arp_response(uint64_t nic, uint32_t ip, uint8_t* mac){
            size_t buffer_size = sizeof(arp_packet_t) + ETH_HEADER_SIZE;
            void *buffer = malloc(buffer_size);

            uint8_t source[6];
            nic_common::get_mac(nic, source);

            size_t offset = network::ethernet::write_header(buffer, mac, source, htons(ETH_EHTERTYPE_ARP));
            arp_packet_t *packet = (arp_packet_t*)((uint64_t)buffer + offset);

            packet->hardware_type = htons(1);
            packet->protocol_type = htons(0x0800); // IPv4
            packet->hardware_len = 6; // MAC
            packet->protocol_len = 4; // IPv4
            packet->opcode = htons(2); // Reply
            memcpy(packet->sender_mac, source, 6);
            packet->sender_ip = nic_common::get_ipv4(nic);
            memcpy(packet->target_mac, mac, 6);
            packet->target_ip = ip;

            nic_common::send(nic, buffer, buffer_size);

            free(buffer);
        }

        void send_arp_request(uint64_t nic, uint32_t ipv4){
            size_t buffer_size = sizeof(arp_packet_t) + ETH_HEADER_SIZE;
            void *buffer = malloc(buffer_size);

            uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            uint8_t source[6];
            nic_common::get_mac(nic, source);

            size_t offset = network::ethernet::write_header(buffer, broadcast, source, htons(ETH_EHTERTYPE_ARP));
            arp_packet_t *packet = (arp_packet_t*)((uint64_t)buffer + offset);

            packet->hardware_type = htons(1);
            packet->protocol_type = htons(0x0800); // IPv4
            packet->hardware_len = 6; // MAC
            packet->protocol_len = 4; // IPv4
            packet->opcode = htons(1); // Request
            memcpy(packet->sender_mac, source, 6);
            packet->sender_ip = nic_common::get_ipv4(nic);
            memset(packet->target_mac, 0, 6);
            packet->target_ip = ipv4;

            nic_common::send(nic, buffer, buffer_size);

            free(buffer);
        }

        void resolve_mac_address(uint64_t nic, uint32_t ipv4, uint8_t *out){
            uint32_t broadcast_address = nic_common::get_broadcast_ip_address(nic);
            if (!broadcast_address) broadcast_address = 0xFFFFFFFF;

            if (ipv4 == broadcast_address || ipv4 == 0xFFFFFFFF){
                out[0] = 0xFF; out[1] = 0xFF;
                out[2] = 0xFF; out[3] = 0xFF;
                out[4] = 0xFF; out[5] = 0xFF;

                return;
            }

            if (!nic_common::get_state(nic)) return; // Not connected... should not happen but who knows

            // Check if its not a local address
            uint32_t subnet = nic_common::get_subnet_mask(nic);
            uint32_t gateway = nic_common::get_gateway(nic);
            if (nic_common::get_ipv4(nic) != 0 && gateway && ((ipv4 & subnet) != (nic_common::get_ipv4(nic) & subnet))){
                // Send it to the gateway instead (Internet access)
                return resolve_mac_address(nic, gateway, out);
            }

            // Check the cache list
            kstd::linked_list_t<arp_cache*> *cache = nic_common::get_arp_cache(nic);
            cache->lock();
            for (int i = 0; i < cache->size(); i++){
                arp_cache* entry = cache->get(i);
                if (entry->ip_address != ipv4) continue;

                if ((entry->timestamp + ARP_CACHE_LIFETIME) <= current_time) { // Invalidate it
                    cache->remove(i);
                    delete entry;
                    break;
                }

                memcpy(out, entry->mac_address, 6);
                cache->unlock();
                return;
            }
            cache->unlock();

            // Create a waiter
            task_t *self = task_scheduler::get_current_task();

            arp_waiter* waiter = new arp_waiter;
            waiter->ip = ipv4;
            waiter->nic_id = nic;
            waiter->task = self;

            waiters.lock();
            waiters.add(waiter);
            waiters.unlock();

            // Send the actual request
            send_arp_request(nic, ipv4);

            // Block waiting for the request
            self->ScheduleFor(5000, BLOCKED);

            // If there was a miss, remove the waiter from the list
            waiters.lock();
            for (int i = 0; i < waiters.size(); i++){
                arp_waiter* w = waiters.get(i);

                if (w == waiter){
                    waiters.remove(i);
                    break;
                }
            }
            waiters.unlock();

            // free the waiter
            delete waiter;

            // Re-search the cache
            bool found = false;

            cache->lock();
            for (int i = 0; i < cache->size(); i++){
                arp_cache* entry = cache->get(i);
                if (entry->ip_address != ipv4) continue;

                found = true;
                memcpy(out, entry->mac_address, 6); // Copy it to the out variable
            }
            cache->unlock();

            if (!found){
                out[0] = 0xFF; out[1] = 0xFF; out[2] = 0xFF;
                out[3] = 0xFF; out[4] = 0xFF; out[5] = 0xFF;
            }
        }
    }
}