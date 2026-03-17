#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/spinlock/spinlock.h>

#define NVME_CAP_REG                0x00 /* 0x00 - 0x07 | Capabilities */
#define NVME_VER_REG                0x08 /* 0x08 - 0x0B | Version */
#define NVME_INTMS_REG              0x0C /* 0x0C - 0x0F | Interrupt Mask Set */
#define NVME_INTMC_REG              0x10 /* 0x10 - 0x13 | Interrupt Mask Clear */
#define NVME_CC_REG                 0x14 /* 0x14 - 0x17 | Controller Configuration */
#define NVME_CSTS_REG               0x1C /* 0x1C - 0x1F | Controller Status */
#define NVME_AQA_REG                0x24 /* 0x24 - 0x27 | Admin queue attributes */
#define NVME_ASQ_REG                0x28 /* 0x28 - 0x2F | Admin submission queue */
#define NVME_ACQ_REG                0x30 /* 0x30 - 0x37 | Admin completion queue */
#define NVME_DOORBELL_STRIDE(dstrd)  (4 << (dstrd))
#define NVME_SQTDBL(qid, dstrd)  (0x1000 + ((2 * (qid)) * NVME_DOORBELL_STRIDE(dstrd)))
#define NVME_CQHDBL(qid, dstrd)  (0x1000 + (((2 * (qid)) + 1) * NVME_DOORBELL_STRIDE(dstrd)))

#define NVME_CAPS_SET_ADMIN      (1 << 6)
#define NVME_CAPS_SET_NVM        (1 << 0)

#define NVME_CC_EN                  (1 << 0)

#define NVME_CC_COMMAND_SET_NVM     0
#define NVME_CC_COMMAND_SET_ADMIN   (0b110 << 4)

#define NVME_CC_MPS(x)              (x << 7) // 0 = 4096 bytes
#define NVME_CC_AMS(x)              (x << 11)
#define NVME_CC_SHN(x)              (x << 14) // Shutdown Notification, 01b = Normal | 10b = Abrupt
#define NVME_CC_IO_SQES(x)          (x << 16) // I/O Submission Queue Entry Size. Log2(bytes). (Usually 6 for 64 bytes).
#define NVME_CC_IO_CQES(x)          (x << 20) // I/O Completion Queue Entry Size. Log2(bytes). (Usually 4 for 16 bytes).

#define NVME_CSTS_RDY               (1 << 0) // Controller Ready.
#define NVME_CSTS_CFS               (1 << 1) // Controller Fatal.
#define NVME_CSTS_SHST              (3 << 2) // Shutdown Status. 00b = Normal, 10b = Shutdown Complete.
#define NVME_CSTS_NSSRO             (1 << 4) // NVM Subsystem Reset Occurred. (Cleared by reading).
#define NVME_CSTS_PP                (1 << 5) // Processing Paused.





typedef union {
    uint64_t raw;
    struct {
        uint64_t max_queue_entries_supported : 16;
        uint64_t contiguous_queues_required : 1;
        uint64_t arbitration_mechanism_supported : 2;
        uint64_t rsv : 5;
        uint64_t timeout : 8;
        uint64_t doorbell_stride : 4;
        uint64_t nvm_subsystem_reset_supported : 1;
        uint64_t command_sets_supported : 8;
        uint64_t boot_partition_support : 1;
        uint64_t controller_power_scope : 2;
        uint64_t memory_page_size_min : 4;
        uint64_t memory_page_size_max : 4;
        uint64_t persistant_memory_region_supported : 1;
        uint64_t controller_memory_buffer_supported : 1;
        uint64_t nvm_subsystem_shutdown_supported : 1;
        uint64_t controller_ready_with_media_support : 1;
        uint64_t rsv2 : 4;
    } __attribute__((packed)) bits;
} nvme_cap_t;

typedef union {
    uint32_t raw;
    struct {
        uint32_t asq_depth : 12; // 0-based (Value 63 = 64 entries)
        uint32_t reserved1 : 4;
        uint32_t acq_depth : 12; // 0-based (Value 63 = 64 entries)
        uint32_t reserved2 : 4;
    } __attribute__((packed)) bits;
} nvme_aqa_t;


struct nvme_submission_entry_t{
    uint32_t command;
    uint32_t namespace_identifier;
    uint64_t rsv;
    uint64_t metadata_ptr;
    uint64_t data_pointers[2]; // PRPs
    uint32_t cmd_specific[6];
} __attribute__((packed));

struct nvme_completion_entry_t{
    uint32_t command_specific;
    uint32_t rsv;
    uint16_t submission_queue_head_ptr;
    uint16_t submission_queue_identifier;
    uint16_t command_identifier;
    uint16_t phase_status; // phase is bit 0, status 1 - 15
} __attribute__((packed));

struct nvme_queue_set{
    spinlock_t submission_lock;
    spinlock_t completion_lock;

    nvme_submission_entry_t* submission_queue;
    uint16_t submission_queue_tail; // owned by the controller
    uint16_t submission_queue_size;

    nvme_completion_entry_t* completion_queue;
    uint16_t completion_queue_head;
    uint16_t completion_queue_phase; // Phase bit
    uint16_t completion_queue_size;

    int queue_id;
    uint8_t command_id;
};