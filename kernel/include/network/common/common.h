#pragma once
#include <stdint.h>
#include <stddef.h>
#include <filesystem/vfs/vfs.h>
#include <network/ethernet/ethernet.h>
#include <network/dhcp/dhcp.h>
#include <network/arp/arp.h>
#include <network/IP/IP.h>
#include <network/UDP/UDP.h>
#include <network/ICMP/ICMP.h>
#include <structures/lists/linked_list.h>

struct arp_cache{
    uint32_t ip_address;
    uint8_t mac_address[6];
    uint64_t timestamp;
};

class inet_socket_t;
struct waiting_socket_t{
    int ref_cnt;
    inet_socket_t *socket;
    uint32_t address;
};

namespace nic_common {
    extern kstd::linked_list_t<waiting_socket_t*> waiting_sockets;
    uint64_t register_nic(vnode_t *node, uint8_t *mac);
    void nic_handle_packet(uint64_t id, void *buffer, size_t size);
    void refresh_nic(bool connected, uint64_t id);
    void get_mac(uint64_t id, uint8_t *out);
    uint32_t get_ipv4(uint64_t id);
    void set_ipv4(uint64_t id, uint32_t ip);
    size_t send(uint64_t id, void *buffer, size_t size);
    uint32_t get_rebinding_time(uint64_t id);
    void set_rebinding_time(uint64_t id, uint32_t value);
    uint32_t get_renewal_time(uint64_t id);
    void set_renewal_time(uint64_t id, uint32_t value);
    uint32_t get_lease_time(uint64_t id);
    void set_lease_time(uint64_t id, uint32_t value);
    uint32_t get_broadcast_ip_address(uint64_t id);
    void set_broadcast_ip_address(uint64_t id, uint32_t value);
    uint32_t get_dns_ip_address(uint64_t id);
    void set_dns_ip_address(uint64_t id, uint32_t value);
    uint32_t get_subnet_mask(uint64_t id);
    void set_subnet_mask_address(uint64_t id, uint32_t value);
    uint32_t get_dhcp_server_address(uint64_t id);
    void set_dhcp_server_address(uint64_t id, uint32_t value);
    void set_gateway(uint64_t id, uint32_t ip);
    uint32_t get_gateway(uint64_t id);
    kstd::linked_list_t<arp_cache*> *get_arp_cache(int id);
    uint64_t get_nic_serving_ip(uint32_t ip);
    bool get_state(uint64_t nic);
    void set_state(uint64_t nic, bool state);
}


inline uint16_t htons(uint16_t value) {
    return (value << 8) | (value >> 8);
}

inline uint16_t ntohs(uint16_t value) {
    return (value << 8) | (value >> 8);
}

inline uint32_t htonl(uint32_t v) {
    return ((v<<24)&0xff000000)|((v<<8)&0x00ff0000)|((v>>8)&0x0000ff00)|((v>>24)&0x000000ff);
}

inline uint32_t ntohl(uint32_t v) {
    return ((v<<24)&0xff000000)|((v<<8)&0x00ff0000)|((v>>8)&0x0000ff00)|((v>>24)&0x000000ff);
}