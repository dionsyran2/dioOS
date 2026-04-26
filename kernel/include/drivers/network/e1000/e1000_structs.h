#pragma once
#include <stdint.h>
#include <stddef.h>

#define E1000_TX_CMD_EOP    (1 << 0)
#define E1000_TX_CMD_IFCS   (1 << 1)
#define E1000_TX_CMD_IC     (1 << 2)
#define E1000_TX_CMD_RS     (1 << 3)
#define E1000_TX_CMD_RPS    (1 << 4)
#define E1000_TX_CMD_DEXT   (1 << 5)
#define E1000_TX_CMD_VLE    (1 << 6)
#define E1000_TX_CMD_IDE    (1 << 7)

#define E1000_TX_STS_DD     (1 << 0)
#define E1000_TX_STS_EC     (1 << 1)
#define E1000_TX_STS_LC     (1 << 2)
#define E1000_TX_STS_TU     (1 << 3)

struct transmit_descriptor_t{
    uint64_t buffer_address;

    uint16_t length;
    uint8_t checksum_offset;
    uint8_t command;
    uint8_t status; // Higher 4 bits are reserved!
    uint8_t checksum_start_field;
    uint16_t special;
} __attribute__((packed));


#define E1000_RX_STS_DD     (1 << 0)
#define E1000_RX_STS_EOP    (1 << 1)
#define E1000_RX_STS_IXSM   (1 << 2)
#define E1000_RX_STS_VP     (1 << 3)
#define E1000_RX_STS_TCPCS  (1 << 5)
#define E1000_RX_STS_IPCS   (1 << 6)
#define E1000_RX_STS_PIF    (1 << 7)

struct receive_descriptor_t{
    uint64_t buffer_address;

    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t error;
    uint16_t special;
} __attribute__((packed));