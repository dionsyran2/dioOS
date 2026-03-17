#pragma once
#include <stdint.h>
#include <stddef.h>

struct fat_bpb_t{
    char code[3];
    char oem_ident[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors;
    uint8_t media_descriptor_type;
    uint16_t sectors_per_fat; // FAT12/16 only
    
    // Disk Geometry
    uint16_t sectors_per_track;
    uint16_t head_count;

    // More bpb stuff
    uint32_t hidden_sector_count;
    uint32_t large_sector_count;
} __attribute__((packed));


#define FAT32_EBR_OFFSET    sizeof(fat_bpb_t) // The EBR starts after the BPB
struct fat32_extended_boot_record_t
{
    uint32_t sectors_per_fat; // The size of the FAT in sectors
    uint16_t flags;
    uint16_t fat_version;
    uint32_t root_dir_cluster; // The cluster of the root directory
    uint16_t fs_info_sector; // The sector number of the FSInfo structure.
    uint16_t backup_boot_sector;
    uint8_t rsv[12];
    uint8_t drive_number;
    uint8_t winnt_flags;
    uint8_t signature; // Signature (must be 0x28 or 0x29). 
    uint8_t volume_id[4];
    char volume_label[11];
    char identifier[8]; // Always "FAT32   "
    uint8_t boot_code[420];
    uint16_t bootable_signature;
} __attribute__((packed));

#define FSINFO_LEAD_SIGNATURE   0x41615252
#define FSINFO_MID_SIGNATURE    0x61417272
#define FSINFO_TRAIL_SIGNATURE  0xAA550000
struct fat32_fsinfo{
    uint32_t lead_signature;
    uint8_t rsv[480];
    uint32_t mid_signature;
    uint32_t free_cluster_count; // Should be marked as -1 when mounted and rewritten when unmounted.
    uint32_t next_free_cluster; // Should not necessarily be trusted
    uint8_t rsv2[12];
    uint32_t trail_signature;
} __attribute__((packed));


#define FAT_RO      0x01
#define FAT_HIDDEN  0x02
#define FAT_SYSTEM  0x04
#define FAT_VOLUME  0x08
#define FAT_DIR     0x10
#define FAT_ARCHIVE 0x20
#define FAT_LFN     (FAT_RO | FAT_HIDDEN | FAT_SYSTEM | FAT_VOLUME)

// winnt_flags
#define FAT_LC_EXTENSION    0x10 // If set, the Extension is lowercase.
#define FAT_LC_BASENAME     0x08 // If set, the Basename (Body) is lowercase.

struct fat_directory_entry_t{
    char name[11]; // 8.3 Filename
    uint8_t attributes;
    uint8_t winnt_flags; // Contain capitalization
    uint8_t creation_time_in_hundredths;

    /* HOUR: 5 bits */
    /* MINS: 6 bits */
    /* SECS: 5 bits NOTE: Multiply seconds by 2 */
    uint16_t creation_time;
    /* Year: 7 bits */
    /* Month: 4 bits */
    /* Day: 5 bits */
    uint16_t creation_date;
    uint16_t last_accessed_date;
    
    uint16_t first_cluster_high;

    uint16_t last_modification_time;
    uint16_t last_modification_date;

    uint16_t first_cluster_low;

    uint32_t file_size;
} __attribute__((packed));

struct fat_long_file_name{
    uint8_t sequence;
    uint16_t name1[5];
    uint8_t attributes;
    uint8_t long_entry_type;
    uint8_t checksum;
    uint16_t name2[6];
    uint16_t rsv;
    uint16_t name3[2];
} __attribute__((packed));

struct fat32_vnode_info_t{
    char sfn_name[11];

    uint32_t parent_start_cluster;

    // Use start_cluster to read the entry
    uint32_t start_cluster;
};
