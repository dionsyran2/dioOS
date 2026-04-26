#include <network/common/common.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <drivers/timers/common.h>
#include <memory.h>
#include <kstdio.h>

struct nic_data{
    int id = 0;
    bool state = false;
    vnode_t *node = nullptr;
    uint8_t mac[6];
    uint32_t ipv4 = 0;

    uint32_t dhcp_server_ip = 0; // The DHCP Server's IP
    uint32_t subnet = 0; // The subnet mask
    uint32_t dns_ip = 0; // The DNS server IP
    uint32_t broadcast_ip = 0; // The network's broadcast IP
    uint32_t lease_time = 0; // Drop all sockets and restart the DHCP Handshake
    uint32_t renewal_time = 0; // T1, ask for a renewal
    uint32_t rebinding_time = 0; // T2, Broadcast a request to renew the ip
    uint32_t gateway_ip = 0;
    
    kstd::linked_list_t<arp_cache*> arp_cache_list;
};

kstd::linked_list_t<nic_data *>* nic_list = nullptr;
int nic_id = 0;

void nic_dhcp_monitor_task(nic_data *data){
    task_t *self = task_scheduler::get_current_task();
    while (true) {
        self->ScheduleFor(5000, BLOCKED);

        if (!data->ipv4) continue;

        if (data->lease_time <= current_time && data->lease_time) {
            // Drop all sockets

            // Re-discover
            data->ipv4 = 0;
            data->broadcast_ip = 0;
            network::dhcp::send_discover(data->id);
        } else if (data->rebinding_time <= current_time && data->rebinding_time) {
            network::dhcp::rebind(data->id);
        } else if (data->renewal_time <= current_time && data->renewal_time){
            network::dhcp::renew(data->id);
        }

    }
}

void nic_dhcp_handshake(uint64_t nic){
    task_t *self = task_scheduler::get_current_task();
    while (nic_common::get_ipv4(nic) == 0 && nic_common::get_state(nic)){
        network::dhcp::send_discover(nic);

        self->ScheduleFor(5000, BLOCKED);
    }

    self->exit(0);
}

namespace nic_common {
    kstd::linked_list_t<waiting_socket_t*> waiting_sockets;

    
    nic_data *get_data(int id){
        nic_list->lock();

        for (int i = 0; i < nic_list->size(); i++){
            nic_data *data = nic_list->get(i);

            if (data && data->id == id){
                nic_list->unlock();
                return data;
            }
        }

        nic_list->unlock();
        return nullptr;
    }

    bool get_state(uint64_t nic){
        nic_data *n = get_data(nic);
        return n->state;
    }

    void set_state(uint64_t nic, bool state){
        nic_data *n = get_data(nic);
        n->state = state;
    }

    uint64_t get_nic_serving_ip(uint32_t ip){
        nic_list->lock();

        for (int i = 0; i < nic_list->size(); i++){
            nic_data *data = nic_list->get(i);

            if (!data || !data->ipv4) continue;

            if ((data->ipv4 & data->subnet) != (ip & data->subnet)) continue;

            uint64_t id = data->id;
            nic_list->unlock();

            return id;
        }

        // Perhaps its is on the internet, return the first one that has an ip & gateway set
        for (int i = 0; i < nic_list->size(); i++){
            nic_data *data = nic_list->get(i);

            if (!data || !data->ipv4 || !data->gateway_ip) continue;

            uint64_t id = data->id;
            nic_list->unlock();
            
            return id;
        }

        nic_list->unlock();

        return -1;
    }

    kstd::linked_list_t<arp_cache*> *get_arp_cache(int id){
        nic_data *nic = get_data(id);
        return &nic->arp_cache_list;
    }

    uint64_t register_nic(vnode_t *node, uint8_t *mac){
        nic_data *nic = new nic_data();
        nic->node = node;
        memcpy(nic->mac, mac, 6);

        nic->id = __atomic_fetch_add(&nic_id, 1, __ATOMIC_SEQ_CST);

        if (nic_list == nullptr){
            nic_list = new kstd::linked_list_t<nic_data *>();
        }

        nic_list->lock();
        nic_list->add(nic);
        nic_list->unlock();

        task_t *task = task_scheduler::create_process("nic_dhcp_task", (function)nic_dhcp_monitor_task, false, false, false);
        task->registers.rdi = (uint64_t)nic;

        task_scheduler::mark_ready(task);
        
        return nic->id;
    }

    void refresh_nic(bool connected, uint64_t id){
        // Reconnect
        set_state(id, connected);

        if (!connected){
            set_ipv4(id, 0);
        } else {
            task_t *task = task_scheduler::create_process("nic_dhcp_handshake", (function)nic_dhcp_handshake, false, false, false);
            task->registers.rdi = id;

            task_scheduler::mark_ready(task);
        }
    }

    void nic_handle_packet_task(uint64_t id, void *buffer, size_t size){
        task_t *self = task_scheduler::get_current_task();

        network::ethernet::handle_packet(id, buffer, size);

        self->exit(0);
    }

    void nic_handle_packet(uint64_t id, void *buffer, size_t size){
        nic_data *nic = get_data(id);

        /* HANDLE PACKET */
        task_t *task = task_scheduler::create_process("nic_packet_handler", (function)nic_handle_packet_task, false, false, false);
        task->registers.rdi = id;
        task->registers.rsi = (uint64_t)buffer;
        task->registers.rdx = size;
        task->priority = 5;

        task_scheduler::mark_ready(task);
    }

    size_t send(uint64_t id, void *buffer, size_t size){
        nic_data *nic = get_data(id);

        return nic->node->write(0, size, buffer);
    }

    void get_mac(uint64_t id, uint8_t *out){
        nic_data *nic = get_data(id);

        memcpy(out, nic->mac, 6);
    }

    uint32_t get_ipv4(uint64_t id){
        nic_data *nic = get_data(id);

        return nic->ipv4;
    }
    
    void set_ipv4(uint64_t id, uint32_t ip){
        nic_data *nic = get_data(id);

        nic->ipv4 = ip;
    }

    uint32_t get_rebinding_time(uint64_t id){
        nic_data *nic = get_data(id);
        return nic->rebinding_time;
    }

    void set_gateway(uint64_t id, uint32_t ip){
        nic_data *nic = get_data(id);

        nic->gateway_ip = ip;
    }

    uint32_t get_gateway(uint64_t id){
        nic_data *nic = get_data(id);
        return nic->gateway_ip;
    }

    void set_rebinding_time(uint64_t id, uint32_t value){
        nic_data *nic = get_data(id);
        nic->rebinding_time = value;
    }

    uint32_t get_renewal_time(uint64_t id){
        nic_data *nic = get_data(id);
        return nic->renewal_time;
    }

    void set_renewal_time(uint64_t id, uint32_t value){
        nic_data *nic = get_data(id);
        nic->renewal_time = value;
    }

    uint32_t get_lease_time(uint64_t id){
        nic_data *nic = get_data(id);
        return nic->lease_time;
    }

    void set_lease_time(uint64_t id, uint32_t value){
        nic_data *nic = get_data(id);
        nic->lease_time = value;
    }


    uint32_t get_broadcast_ip_address(uint64_t id){
        nic_data *nic = get_data(id);
        return nic->broadcast_ip;
    }

    void set_broadcast_ip_address(uint64_t id, uint32_t value){
        nic_data *nic = get_data(id);
        nic->broadcast_ip = value;
    }

    uint32_t get_dns_ip_address(uint64_t id){
        nic_data *nic = get_data(id);
        return nic->dns_ip;
    }

    void set_dns_ip_address(uint64_t id, uint32_t value){
        nic_data *nic = get_data(id);
        nic->dns_ip = value;
    }

    uint32_t get_subnet_mask(uint64_t id){
        nic_data *nic = get_data(id);
        return nic->subnet;
    }

    void set_subnet_mask_address(uint64_t id, uint32_t value){
        nic_data *nic = get_data(id);
        nic->subnet = value;
    }
    
    uint32_t get_dhcp_server_address(uint64_t id){
        nic_data *nic = get_data(id);
        return nic->dhcp_server_ip;
    }

    void set_dhcp_server_address(uint64_t id, uint32_t value){
        nic_data *nic = get_data(id);
        nic->dhcp_server_ip = value;
    }
}