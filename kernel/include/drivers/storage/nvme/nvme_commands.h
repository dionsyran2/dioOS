#pragma once
#include <stdint.h>
#include <stddef.h>
/* COMMANDS */
// Opcode 0x01: Create I/O Submission Queue
// Opcode 0x05: Create I/O Completion Queue
#define NVME_ADMIN_CMD_CREATE_SQ                0x01
#define NVME_ADMIN_CMD_CREATE_CQ                0x05

#define NVME_ADMIN_CMD_IDENTIFY                 0x06

#define NVME_IO_CMD_WRITE                       0x01
#define NVME_IO_CMD_READ                        0x02

// Identify params
#define NVME_ADMIN_IDENTIFY_PARAM_NS            0x00
#define NVME_ADMIN_IDENTIFY_PARAM_CONTROLLER    0x01
#define NVME_ADMIN_IDENTIFY_PARAM_NSLIST        0x02


typedef struct {
    uint16_t vendor_id;
    uint16_t ss_vendor_id;
    uint8_t  serial_number[20];
    uint8_t  model_number[40];
    uint8_t  firmware_revision[8];
    uint8_t  rab;
    uint8_t  ieee_oui[3];
    uint8_t  cmic;
    uint8_t  mdts; // Max Data Transfer Size (Power of 2 units of Min Memory Page Size)
    uint16_t cntlid;
    uint32_t ver;
    uint32_t rtd3r;
    uint32_t rtd3e;
    uint32_t oaes;
    uint32_t ctratt;
    uint8_t  rsvd140[12];
    uint8_t  oacs; // Optional Admin Command Support
    uint8_t  acl;
    uint8_t  aerl;
    uint8_t  frmw;
    uint8_t  lpa;
    uint8_t  elpe;
    uint8_t  npss;
    uint8_t  avscc;
    uint8_t  apsta;
    uint16_t wctemp;
    uint16_t cctemp;
    uint16_t mtfa;
    uint32_t hmpre;
    uint32_t hmmin;
    uint8_t  tnvmcap[16];
    uint8_t  unvmcap[16];
    uint32_t rpmbs;
    uint16_t edstt;
    uint8_t  dsto;
    uint8_t  fwug;
    uint16_t kas;
    uint16_t hctma;
    uint16_t mntmt;
    uint16_t mxtmt;
    uint32_t sanicap;
    uint32_t hmminds;
    uint16_t hmmaxd;
    uint16_t nsetidmax;
    uint16_t endgidmax;
    uint8_t  anatt;
    uint8_t  anacap;
    uint32_t anagrpmax;
    uint32_t nanagrpid;
    uint32_t pels;
    uint8_t  rsvd352[164];
    uint8_t  sqes; // SQ Entry Size (Required)
    uint8_t  cqes; // CQ Entry Size (Required)
    uint16_t maxcmd;
    uint32_t nn; // Number of Namespaces
    uint16_t oncs; // Optional NVM Command Support
    uint16_t fuses;
    uint8_t  fna;
    uint8_t  vwc;
    uint16_t awun;
    uint16_t awupf;
    uint8_t  nvscc;
    uint8_t  nwpc;
    uint16_t acwu;
    uint8_t  rsvd534[2];
    uint32_t sgls;
    uint32_t mnan;
    uint8_t  rsvd544[224];
    uint8_t  subnqn[256];
    uint8_t  rsvd1024[2048];
    uint8_t  psd[1024]; // Power State Descriptors
} __attribute__((packed)) nvme_identify_controller_t;

typedef struct {
    uint16_t metadata_size;       // MS (Bytes of extra metadata per block)
    uint8_t  lba_data_size;       // LBADS (Power of 2. e.g., 9=512B, 12=4KB)
    uint8_t  relative_performance : 2; // RP (0=Best, 3=Worst)
    uint8_t  reserved : 6;
} __attribute__((packed)) nvme_lba_format_t;


typedef struct {
    uint64_t ns_size;
    uint64_t ns_capacity;
    uint64_t ns_use;
    uint8_t  ns_features;
    uint8_t  num_lba_formats;
    uint8_t  formatted_lba_size;
    uint8_t  mc;
    uint8_t  dpc;
    uint8_t  dps;
    uint8_t  nmic;
    uint8_t  rescap;
    uint8_t  fpi;
    uint8_t  dlfeat;

    uint8_t  rsvd34[2];

    uint16_t nawun;
    uint16_t nawupf;
    uint16_t nacwu;
    uint16_t nabsn;
    uint16_t nabo;
    uint16_t nabspf;
    uint16_t noiob;

    uint8_t  rsvd50[14];

    uint8_t  nvmcap[16];

    uint8_t  npwg; 
    uint8_t  npwa; 
    uint16_t npdg; 
    uint8_t  npda; 
    uint8_t  nows; 


    uint8_t  rsvd86[6];

    uint32_t anagrpid;
    uint8_t  rsvd96[3];
    uint8_t  nsattr;
    uint16_t nvmsetid;
    uint16_t endgid;
    
    uint8_t  nguid[16];
    uint8_t  eui64[8];

    nvme_lba_format_t lba_formats[16]; 

    uint8_t  rsvd192[192];
    uint8_t  vs[3712];
} __attribute__((packed)) nvme_identify_ns_t;

static_assert(offsetof(nvme_identify_ns_t, lba_formats) == 128, "Struct Alignment Error: lba_formats must be at offset 128");
static_assert(sizeof(nvme_identify_ns_t) == 4096, "Struct Size Error: Must be 4096 bytes");