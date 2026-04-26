#include <network/UDP/UDP.h>
#include <structures/lists/linked_list.h>
#include <memory.h>
#include <kstdio.h>


struct port_t{
    void (*cb)(void *buffer, size_t size, uint64_t nic_id);
};

struct udp_nic_info{
    uint64_t identifier;
    port_t *ports[UINT16_MAX + 1];

    uint16_t next_ephimeral;
};

kstd::linked_list_t<udp_nic_info*>* udp_nic_list = nullptr; 

namespace network{
    namespace UDP{
        udp_nic_info* _get_nic(uint64_t nic_id){
            if (udp_nic_list == nullptr) udp_nic_list = new kstd::linked_list_t<udp_nic_info*>();

            udp_nic_list->lock();

            for (int i = 0; i < udp_nic_list->size(); i++){
                udp_nic_info *data = udp_nic_list->get(i);

                if (data && data->identifier == nic_id){
                    udp_nic_list->unlock();
                    return data;
                }
            }

            udp_nic_info *ret = new udp_nic_info;
            memset(ret, 0, sizeof(udp_nic_info));
            ret->identifier = nic_id;
            ret->next_ephimeral = 49152;
            
            udp_nic_list->add(ret);

            udp_nic_list->unlock();

            return ret;
        }

        uint16_t allocate_port(uint64_t nic_id, uint16_t req, void (*cb)(void *buffer, size_t size, uint64_t nic_id)){
            udp_nic_info* nic = _get_nic(nic_id);

            if (req && nic->ports[req] != nullptr){
                return 0;
            }

            req = req ? req : __atomic_fetch_add(&nic->next_ephimeral, 1, __ATOMIC_SEQ_CST);
            nic->ports[req] = new port_t;

            nic->ports[req]->cb = cb;

            return req;
        }

        size_t write_header(int nic_id, void *buffer, uint32_t dest_ip, uint16_t source_port, uint16_t dst_port, uint16_t payload_size){
            size_t ip_size = network::IP::write_header(nic_id, buffer, IP_PROTOCOL_UDP, nic_common::get_ipv4(nic_id), dest_ip, sizeof(udp_header_t) + payload_size);
            buffer = (void*)((uint64_t)buffer + ip_size);
            udp_header_t *header = (udp_header_t*)buffer;

            header->src_port = htons(source_port);
            header->dst_port = htons(dst_port);
            header->length = htons(sizeof(udp_header_t) + payload_size);
            header->checksum = 0;
            //header->checksum = _calculate_checksum(header);

            return ip_size + sizeof(udp_header_t);
        }

        void handle_packet(uint64_t id, void *buffer, size_t size){
            udp_header_t *header = (udp_header_t*)buffer;
            
            uint16_t dest = ntohs(header->dst_port);

            udp_nic_info* nic = _get_nic(id);

            if (nic && nic->ports[dest] && nic->ports[dest]->cb){
                nic->ports[dest]->cb((void*)((uint64_t)buffer + sizeof(udp_header_t)), size - sizeof(udp_header_t), id);
            }
        }
    }
}