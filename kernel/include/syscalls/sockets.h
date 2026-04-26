#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <structures/lists/linked_list.h>
#include <syscalls/syscalls.h>
#include <memory/heap.h>
#include <network/common/common.h>

struct msghdr {
    void *msg_name;
    uint32_t msg_namelen;
    iovec *msg_iov;
    uint64_t msg_iovlen;
    void *msg_control;
    uint64_t msg_controllen;
    int msg_flags;
};


/* Protocol families.  */
#define PF_UNSPEC	0	/* Unspecified.  */
#define PF_LOCAL	1	/* Local to host (pipes and file-domain).  */
#define PF_UNIX		PF_LOCAL /* POSIX name for PF_LOCAL.  */
#define PF_FILE		PF_LOCAL /* Another non-standard name for PF_LOCAL.  */
#define PF_INET		2	/* IP protocol family.  */
#define PF_AX25		3	/* Amateur Radio AX.25.  */
#define PF_IPX		4	/* Novell Internet Protocol.  */
#define PF_APPLETALK	5	/* Appletalk DDP.  */
#define PF_NETROM	6	/* Amateur radio NetROM.  */
#define PF_BRIDGE	7	/* Multiprotocol bridge.  */
#define PF_ATMPVC	8	/* ATM PVCs.  */
#define PF_X25		9	/* Reserved for X.25 project.  */
#define PF_INET6	10	/* IP version 6.  */
#define PF_ROSE		11	/* Amateur Radio X.25 PLP.  */
#define PF_DECnet	12	/* Reserved for DECnet project.  */
#define PF_NETBEUI	13	/* Reserved for 802.2LLC project.  */
#define PF_SECURITY	14	/* Security callback pseudo AF.  */
#define PF_KEY		15	/* PF_KEY key management API.  */
#define PF_NETLINK	16
#define PF_ROUTE	PF_NETLINK /* Alias to emulate 4.4BSD.  */
#define PF_PACKET	17	/* Packet family.  */
#define PF_ASH		18	/* Ash.  */
#define PF_ECONET	19	/* Acorn Econet.  */
#define PF_ATMSVC	20	/* ATM SVCs.  */
#define PF_RDS		21	/* RDS sockets.  */
#define PF_SNA		22	/* Linux SNA Project */
#define PF_IRDA		23	/* IRDA sockets.  */
#define PF_PPPOX	24	/* PPPoX sockets.  */
#define PF_WANPIPE	25	/* Wanpipe API sockets.  */
#define PF_LLC		26	/* Linux LLC.  */
#define PF_IB		27	/* Native InfiniBand address.  */
#define PF_MPLS		28	/* MPLS.  */
#define PF_CAN		29	/* Controller Area Network.  */
#define PF_TIPC		30	/* TIPC sockets.  */
#define PF_BLUETOOTH	31	/* Bluetooth sockets.  */
#define PF_IUCV		32	/* IUCV sockets.  */
#define PF_RXRPC	33	/* RxRPC sockets.  */
#define PF_ISDN		34	/* mISDN sockets.  */
#define PF_PHONET	35	/* Phonet sockets.  */
#define PF_IEEE802154	36	/* IEEE 802.15.4 sockets.  */
#define PF_CAIF		37	/* CAIF sockets.  */
#define PF_ALG		38	/* Algorithm sockets.  */
#define PF_NFC		39	/* NFC sockets.  */
#define PF_VSOCK	40	/* vSockets.  */
#define PF_KCM		41	/* Kernel Connection Multiplexor.  */
#define PF_QIPCRTR	42	/* Qualcomm IPC Router.  */
#define PF_SMC		43	/* SMC sockets.  */
#define PF_XDP		44	/* XDP sockets.  */
#define PF_MCTP		45	/* Management component transport protocol.  */
#define PF_MAX		46	/* For now..  */

/* Address families */
#define AF_UNSPEC	PF_UNSPEC
#define AF_LOCAL	PF_LOCAL
#define AF_UNIX		PF_UNIX
#define AF_FILE		PF_FILE
#define AF_INET		PF_INET
#define AF_AX25		PF_AX25
#define AF_IPX		PF_IPX
#define AF_APPLETALK	PF_APPLETALK
#define AF_NETROM	PF_NETROM
#define AF_BRIDGE	PF_BRIDGE
#define AF_ATMPVC	PF_ATMPVC
#define AF_X25		PF_X25
#define AF_INET6	PF_INET6
#define AF_ROSE		PF_ROSE
#define AF_DECnet	PF_DECnet
#define AF_NETBEUI	PF_NETBEUI
#define AF_SECURITY	PF_SECURITY
#define AF_KEY		PF_KEY
#define AF_NETLINK	PF_NETLINK
#define AF_ROUTE	PF_ROUTE
#define AF_PACKET	PF_PACKET
#define AF_ASH		PF_ASH
#define AF_ECONET	PF_ECONET
#define AF_ATMSVC	PF_ATMSVC
#define AF_RDS		PF_RDS
#define AF_SNA		PF_SNA
#define AF_IRDA		PF_IRDA
#define AF_PPPOX	PF_PPPOX
#define AF_WANPIPE	PF_WANPIPE
#define AF_LLC		PF_LLC
#define AF_IB		PF_IB
#define AF_MPLS		PF_MPLS
#define AF_CAN		PF_CAN
#define AF_TIPC		PF_TIPC
#define AF_BLUETOOTH	PF_BLUETOOTH
#define AF_IUCV		PF_IUCV
#define AF_RXRPC	PF_RXRPC
#define AF_ISDN		PF_ISDN
#define AF_PHONET	PF_PHONET
#define AF_IEEE802154	PF_IEEE802154
#define AF_CAIF		PF_CAIF
#define AF_ALG		PF_ALG
#define AF_NFC		PF_NFC
#define AF_VSOCK	PF_VSOCK
#define AF_KCM		PF_KCM
#define AF_QIPCRTR	PF_QIPCRTR
#define AF_SMC		PF_SMC
#define AF_XDP		PF_XDP
#define AF_MCTP		PF_MCTP
#define AF_MAX		PF_MAX


#define SOCK_STREAM         1
#define SOCK_DGRAM          2
#define SOCK_RAW            3
#define SOCK_RDM            4
#define SOCK_SEQPACKET      5
#define SOCK_DCCP           6
#define SOCK_PACKET         10
#define SOCK_CLOEXEC        00004000
#define SOCK_NONBLOCK       00004000

#define IPPROTO_IP		    1
#define IPPROTO_ICMP		1
#define IPPROTO_IGMP		2
#define IPPROTO_IPIP		4
#define IPPROTO_TCP		    6
#define IPPROTO_EGP		    8
#define IPPROTO_PUP		    12
#define IPPROTO_UDP		    17
#define IPPROTO_IDP		    22
#define IPPROTO_TP		    29
#define IPPROTO_DCCP		33
#define IPPROTO_IPV6		41
#define IPPROTO_RSVP		46
#define IPPROTO_GRE		    47
#define IPPROTO_ESP		    50
#define IPPROTO_AH		    51
#define IPPROTO_MTP		    92
#define IPPROTO_BEETPH		94
#define IPPROTO_ENCAP		98
#define IPPROTO_PIM		    103
#define IPPROTO_COMP		108
#define IPPROTO_SCTP		132
#define IPPROTO_UDPLITE		136
#define IPPROTO_MPLS		137
#define IPPROTO_ETHERNET	143
#define IPPROTO_RAW		    255
#define IPPROTO_MPTCP		262

struct sockaddr{
    uint16_t family;
    char data[14];
} __attribute__((packed));

struct sockaddr_un {
    uint16_t sun_family;
    char sun_path[];
} __attribute__((packed));

struct in_addr {
    uint32_t s_addr; // e.g., 0x08080808 for 8.8.8.8
};

// The main struct (Exactly 16 bytes long)
struct sockaddr_in {
    uint16_t   sin_family;   // Address family (Always AF_INET = 2)
    uint16_t   sin_port;     // 16-bit Port number (e.g., 80 for HTTP)
    struct in_addr sin_addr; // 32-bit IPv4 Address
    char       sin_zero[8];  // 8 bytes of padding to reach 16 bytes
} __attribute__((packed));

/* INTERNAL */
enum socket_state{
    NONE,
    CONNECTED,
    HOST,
    LISTENING,
};

class unix_socket_t;

struct pending_connection {
    unix_socket_t *socket;
    task_t *task; // To wake it up on accept
    
    pending_connection *next;

    int status; // 0 is success, < 0 for failure
};

struct socket_transfer{
    void *buffer = nullptr;
    size_t offset = 0;
    size_t buffer_size = 0;
    
    bool pollin = false;
    bool pollout = false;

    bool eof = false;

    spinlock_t slock = 0;
    uint64_t rflags;

    int ref_cnt = 0;

    task_t *waiter;

    void wait(task_t *self);

    void open();
    void close();

    void lock();
    void unlock();

    void clean();

    long write(const void *buffer, size_t size);
    long read(void *buffer, size_t size);
};

class socket_t{
    public:
    vnode_t *file;

    int domain;
    int type;
    int protocol;

    // Virtual destructor is mandatory so 'delete socket' cleans up the right memory
    virtual ~socket_t() = default; 

    // --- CONNECTIONLESS (UDP, ICMP, UNIX DGRAM) ---
    // By default, these return an error (-1) unless the child class overwrites them.
    virtual int64_t sendto(const void *buf, size_t size, int flags, const struct sockaddr *dest_addr, uint64_t addrlen) { return this->send(buf, size, flags); }
    virtual int64_t recvfrom(void *buf, size_t size, int flags, const struct sockaddr *dest_addr, uint64_t addrlen) { return this->recv(buf, size, flags); }

    virtual int connect(sockaddr *addr, uint64_t addrlen) { return -1; }
    virtual int64_t send(const void* buf, size_t len, uint64_t flags) { return -1; }
    virtual int64_t recv(void* buf, size_t len, uint64_t flags) { return -1; }

    // --- SERVER STUFF ---
    virtual int bind(const struct sockaddr *addr, uint32_t addrlen) { return -1; }
    virtual int listen() { return -1; }
    virtual vnode_t* accept() { return nullptr; }

    // Polling
    virtual bool pollin() { return false; }
    virtual bool pollout() { return false; }
};

class unix_socket_t : public socket_t {
    public:
    socket_transfer *inbound = nullptr;
    socket_transfer *outbound = nullptr;
    socket_state state;

    kstd::linked_list_t<pending_connection *> pending_list;
    task_t *sleeper = nullptr;
    
    int64_t send(const void* buf, size_t len, uint64_t flags);
    int64_t recv(void* buf, size_t len, uint64_t flags);

    int connect(sockaddr *addr, uint64_t addrlen);
    int bind(const struct sockaddr *addr, uint32_t addrlen);
    int listen();
    vnode_t* accept();
    bool pollin();
    bool pollout();
};


struct inbound_packet{
    uint32_t source_ip = 0;
    void *buffer = nullptr;
    size_t length = 0;
};


class inet_socket_t : public socket_t {
    public:
    kstd::linked_list_t<inbound_packet> inbound_packets;
    task_t *sleeper = nullptr;
    int64_t sendto(const void *buf, size_t size, int flags, const struct sockaddr *dest_addr, uint64_t addrlen);
    int64_t recvfrom(void *buf, size_t size, int flags, const struct sockaddr *dest_addr, uint64_t addrlen);
    bool pollin();
    bool pollout();

    ~inet_socket_t(){
        nic_common::waiting_sockets.lock();
        for (int i = 0; i < nic_common::waiting_sockets.size(); i++){
            waiting_socket_t *ws = nic_common::waiting_sockets.get(i);

            if (ws->socket == this){
                nic_common::waiting_sockets.remove(i);
                i--;
                delete ws;
            }
        }
        nic_common::waiting_sockets.unlock();

        while (inbound_packets.size()){
            void *buffer = inbound_packets.get(0).buffer;
            if (buffer) free(buffer);

            inbound_packets.remove(0);
        }
    }
};