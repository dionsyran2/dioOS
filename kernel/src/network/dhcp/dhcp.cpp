#include <network/dhcp/dhcp.h>
#include <drivers/timers/common.h>
#include <memory/heap.h>
#include <random.h>
#include <memory.h>
#include <kstdio.h>

namespace network{
    namespace dhcp{
        void request_ip(uint64_t nic, uint32_t dhcp_ip, uint32_t req_ip, uint32_t xid);
        void dhcp_cb(void *buffer, size_t size, uint64_t nic_id){
            dhcp_packet_t *packet = (dhcp_packet_t *)((uint64_t)buffer);

            if (packet->op != DHCP_OP_RES) return;

            uint8_t message_type = 0;
            uint32_t dhcp_server = 0;
            uint32_t subnet_mask = 0;
            uint32_t dns = 0;
            uint32_t broadcast_address = 0;
            uint32_t ip = packet->yiaddr;
            uint32_t ip_lease_time = 0;
            uint32_t renewal_time = 0;
            uint32_t rebinding_time = 0;
            uint32_t gateway = 0;

            for (int i = 0; true; i += 2){
                uint8_t option = packet->options[i];
                uint8_t length = packet->options[i + 1];

                if (option == 0xFF) break; // End
                if (option == 0){
                    i--;
                    continue;
                }

                switch (option){
                    case 53: // Message Type
                        message_type = packet->options[i + 2];
                        break;
                    case 54: // DHCP Server Identifier
                        dhcp_server = *((uint32_t*)&packet->options[i + 2]);
                        break;
                    case 51: // IP Lease Time
                        ip_lease_time = htonl(*((uint32_t*)&packet->options[i + 2]));
                        break;
                    case 58: // Renewal Time
                        renewal_time = htonl(*((uint32_t*)&packet->options[i + 2]));
                        break;
                    case 59: // Rebinding Time
                        rebinding_time = htonl(*((uint32_t*)&packet->options[i + 2]));
                        break;
                    case 1: // Subnet Mask
                        subnet_mask = *((uint32_t*)&packet->options[i + 2]);
                        break;
                    case 3: // Gateway
                        gateway = *((uint32_t*)&packet->options[i + 2]);
                        break;
                    case 6: // DNS
                        dns = *((uint32_t*)&packet->options[i + 2]);
                        break;
                    case 28:
                        broadcast_address = *((uint32_t*)&packet->options[i + 2]);
                        break;
                }

                i += length;
            }


            if (message_type == 2 /* Offer */ ){
                request_ip(nic_id, dhcp_server, ip, packet->xid);
            } else if (message_type == 5 /* ACK */){
                /*kprintf("DHCP Server: %d.%d.%d.%d\n", dhcp_server & 0xFF, (dhcp_server >> 8) & 0xFF, (dhcp_server >> 16) & 0xFF, dhcp_server >> 24);
                kprintf("Subnet: %d.%d.%d.%d\n", subnet_mask & 0xFF, (subnet_mask >> 8) & 0xFF, (subnet_mask >> 16) & 0xFF, subnet_mask >> 24);
                kprintf("DNS: %d.%d.%d.%d\n", dns & 0xFF, (dns >> 8) & 0xFF, (dns >> 16) & 0xFF, dns >> 24);
                kprintf("Broadcast: %d.%d.%d.%d\n", broadcast_address & 0xFF, (broadcast_address >> 8) & 0xFF, (broadcast_address >> 16) & 0xFF, broadcast_address >> 24);
                kprintf("Offered IP: %d.%d.%d.%d\n", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24);
                kprintf("Lease Time: %d (%d mins)\n", ip_lease_time, ip_lease_time / 60);
                kprintf("Renewal Time: %d (%d mins)\n", renewal_time, renewal_time / 60);
                kprintf("Rebinding Time: %d (%d mins)\n", rebinding_time, rebinding_time / 60);*/
                //kprintf("ACK!\n");


                nic_common::set_dhcp_server_address(nic_id, dhcp_server);
                nic_common::set_subnet_mask_address(nic_id, subnet_mask);
                nic_common::set_gateway(nic_id, gateway);
                nic_common::set_dns_ip_address(nic_id, dns);
                nic_common::set_broadcast_ip_address(nic_id, broadcast_address);
                nic_common::set_ipv4(nic_id, ip);
                nic_common::set_lease_time(nic_id, current_time + ip_lease_time);
                nic_common::set_renewal_time(nic_id, current_time + renewal_time);
                nic_common::set_rebinding_time(nic_id, current_time + rebinding_time);
            }
        }

        void send_discover(uint64_t nic_id){
            network::UDP::allocate_port(nic_id, 68, dhcp_cb);
            
            size_t total_size = sizeof(dhcp_packet_t) + UDP_HEADER_SIZE + IPV4_HEADER_SIZE + ETH_HEADER_SIZE;
            void *buffer = malloc(total_size);
            size_t offset = network::UDP::write_header(nic_id, buffer, 0xFFFFFFFF /*Broadcast*/, 68, 67 /* Server Port */, sizeof(dhcp_packet_t));
            
            dhcp_packet_t *packet = (dhcp_packet_t *)((uint64_t)buffer + offset);
            memset(packet, 0, sizeof(dhcp_packet_t));

            packet->op = DHCP_OP_REQ; // Request
            packet->htype = 1; // Ethernet
            packet->hlen = 6; // Mac Size
            packet->hops = 0;
            packet->xid = htonl(random());
            packet->secs = 0;
            packet->flags = 0; //htons(0x8000); // Broadcast
            packet->magic_cookie = htonl(0x63825363);


            packet->options[0] = 53;    // Option: DHCP Message Type
            packet->options[1] = 1;     // Length: 1 byte
            packet->options[2] = 1;     // Value: 1 (Discover)
            packet->options[3] = 55;    // Option: Parameter Request List
            packet->options[4] = 3;     // Length
            packet->options[5] = 1;     // Request the subnet mask
            packet->options[6] = 3;     // Gateway IP
            packet->options[7] = 6;     // DNS Server
            packet->options[8] = 255;  // Option: End of options marker (0xFF)

            uint8_t my_mac[6];
            nic_common::get_mac(nic_id, my_mac);
            memcpy(packet->chaddr, my_mac, 6);

            nic_common::send(nic_id, buffer, total_size);

            free(buffer);
        }

        void request_ip(uint64_t nic, uint32_t dhcp_ip, uint32_t req_ip, uint32_t transaction_id){
            size_t total_size = sizeof(dhcp_packet_t) + UDP_HEADER_SIZE + IPV4_HEADER_SIZE + ETH_HEADER_SIZE;
            void *buffer = malloc(total_size);
            size_t offset = network::UDP::write_header(nic, buffer, 0xFFFFFFFF /*Broadcast*/, 68, 67 /* Server Port */, sizeof(dhcp_packet_t));
            
            dhcp_packet_t *packet = (dhcp_packet_t *)((uint64_t)buffer + offset);
            memset(packet, 0, sizeof(dhcp_packet_t));

            packet->op = DHCP_OP_REQ; // Request
            packet->htype = 1; // Ethernet
            packet->hlen = 6; // Mac Size
            packet->hops = 0;
            packet->xid = transaction_id;
            packet->secs = 0;
            packet->flags = 0; //htons(0x8000); // Broadcast
            packet->magic_cookie = htonl(0x63825363);

            packet->options[0] = 53;   // Option: DHCP Message Type
            packet->options[1] = 1;    // Length: 1 byte
            packet->options[2] = 3;    // Value: 3 (Request)

            packet->options[3] = 50;   // Option: Requested IP
            packet->options[4] = 4;
            *((uint32_t*)&packet->options[5]) = req_ip;

            // ADD THIS: Option 54 (Server Identifier)
            packet->options[9] = 54;
            packet->options[10] = 4;
            *((uint32_t*)&packet->options[11]) = dhcp_ip;

            packet->options[15] = 255;

            nic_common::get_mac(nic, packet->chaddr);

            nic_common::send(nic, buffer, total_size);

            free(buffer);
        }

        void renew(uint64_t nic, bool broadcast){
            uint32_t dhcp_address = broadcast ? nic_common::get_broadcast_ip_address(nic) : nic_common::get_dhcp_server_address(nic);

            size_t total_size = sizeof(dhcp_packet_t) + UDP_HEADER_SIZE + IPV4_HEADER_SIZE + ETH_HEADER_SIZE;
            void *buffer = malloc(total_size);
            size_t offset = network::UDP::write_header(nic, buffer, dhcp_address, 68, 67 /* Server Port */, sizeof(dhcp_packet_t));
            
            dhcp_packet_t *packet = (dhcp_packet_t *)((uint64_t)buffer + offset);
            memset(packet, 0, sizeof(dhcp_packet_t));

            packet->op = DHCP_OP_REQ; // Request
            packet->htype = 1; // Ethernet
            packet->hlen = 6; // Mac Size
            packet->hops = 0;
            packet->xid = ntohl(random());
            packet->secs = 0;
            packet->flags = 0;
            packet->magic_cookie = htonl(0x63825363);
            packet->siaddr = dhcp_address;
            packet->ciaddr = nic_common::get_ipv4(nic);

            nic_common::get_mac(nic, packet->chaddr);

            packet->options[0] = 53;   // Option: DHCP Message Type
            packet->options[1] = 1;    // Length: 1 byte
            packet->options[2] = 3;    // Value: 3 (Request)

            packet->options[3] = 255;

            //kprintf("SENDING RENEW REQ (%d)", broadcast);
            nic_common::send(nic, buffer, total_size);

            free(buffer);
        }

        void rebind(uint64_t nic){
            renew(nic, true);
        }
    }
}