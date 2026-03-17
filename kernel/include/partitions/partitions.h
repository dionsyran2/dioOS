#pragma once
#include <stdint.h>
#include <stddef.h>
#include <filesystem/vfs/vfs.h>

// --- General --- //
#define GPT_SIGNATURE "EFI PART"


struct partition_t{
    vnode_t* node;
    uint64_t starting_lba;
    uint64_t ending_lba;
};

// --- GPT --- //

namespace gpt{
    // LBA 1:
    struct partition_table_header_t{
        char signature[8]; // "EFI PART" (45h 46h 49h 20h 50h 41h 52h 54h)
        uint32_t gpt_revision;
        uint32_t header_size;
        uint32_t crc32_checksum;
        uint32_t rsv;
        uint64_t header_lba;
        uint64_t alt_header_lba;

        /* Partitioning Info */
        uint64_t first_usable_block;
        uint64_t last_usable_block;

        /* Disk GUID */
        char disk_guid[16];

        uint64_t partition_entry_table_lba;
        uint32_t partition_count;
        uint32_t partition_entry_size;
        uint32_t crc32_partition_entry_array_checksum;
    };

    // LBA partition_entry_table_lba:
    struct partition_entry_t{
        uint8_t partition_type_guid[16];
        uint8_t partition_guid[16];
        uint64_t starting_lba;
        uint64_t ending_lba;
        uint64_t attributes;
        char partition_name[72];
    };

} // namespace gpt

namespace mbr{
    // MBR Partition Entry Structure (16 bytes)
    struct partition_entry_t {
        uint8_t  status;       // 0x80 = Bootable, 0x00 = Inactive
        uint8_t  chs_start[3]; // Cylinder-Head-Sector start (Legacy, ignore)
        uint8_t  type;         // Partition Type (0x83=Linux, 0x07=NTFS, 0xEE=GPT Protective)
        uint8_t  chs_end[3];   // Cylinder-Head-Sector end (Legacy, ignore)
        uint32_t lba_start;    // LBA of partition start
        uint32_t sector_count; // Number of sectors
    } __attribute__((packed));

    // MBR Sector Structure (512 bytes)
    struct mbr_sector_t {
        uint8_t  bootstrap_code[446];      // Bootloader code (GRUB etc.)
        partition_entry_t entries[4];  // Four primary partitions
        uint16_t signature;                // 0xAA55
    } __attribute__((packed));
} // namespace mbr

void intialize_disk_partitions(vnode_t* blk, bool p_prefix = true);