#pragma once
#include <stdint.h>
#include <stddef.h>

struct ext2_superblock_t{
    uint32_t total_inode_count;
    uint32_t total_block_count;

    uint32_t reserved_block_count; // For superuser

    uint32_t unallocated_block_count;
    uint32_t unallocated_inode_count;

    uint32_t first_data_block;
    
    uint32_t log2_block_size;
    uint32_t log2_fragment_size;

    // number of x in block group
    uint32_t blocks_per_block_group;
    uint32_t fragments_per_block_group;
    uint32_t inodes_per_block_group;

    // Time Keeping ?
    // These are 32 bit values, which means it should be fine till 2038
    uint32_t last_mount_time;
    uint32_t last_written_time;

    uint16_t mount_count;
    uint16_t consistency_check; // The times the fs is allowed to be mounted before a consistency check (fsck)

    // Unlike microsoft, someone though of that.
    uint16_t signature; // Always 0xef53 for EXT2
    
    uint16_t filesystem_state; /* 1: FS is clean. 2: FS has errors */

    uint16_t error_handling; // What to do if an error is detected. (1: Ignore, 2: Remount as RO, 3: Panic)

    uint16_t minor_version;

    // EXT2 Surely cares a lot about consistency
    uint32_t last_consistency_check_time;

    uint32_t force_consistency_check_interval; // Interval between forced consistency checks

    uint32_t creator_os_id; /* 0: Linux, 1: GNU HURD, 2: MASIX, 3: FreeBSD, 4: Other "Lites" */

    uint32_t major_version;

    // There are permissions for that? Oh god
    // User/Group that can reserve blocks
    uint16_t uid_rsv_blocks;
    uint16_t gid_rsv_blocks;
} __attribute__((packed));
static_assert(sizeof(ext2_superblock_t) == 84, "Invalid superblock size");

// Only present if the major version is >= 1
struct ext2_extended_superblock_t{
    uint32_t first_non_reserved_inode; //  In versions < 1.0, this is fixed as 11
    uint16_t inode_size; // In versions < 1.0, this is fixed as 128 

    uint16_t this_block_group; // Block group that this superblock is part of (if backup copy) 

    /* TO-DO: bitfield description*/
    uint32_t optional_features;

    /* bit 1: Compression is used 
     * bit 2: Directory Entries contain a type field
     * bit 3: File system needs to replay its journal?
     * bit 4: File system uses a journal device 
    */
    uint32_t required_features;
    
    /* bit 1: Sparce superblocks and descriptor tables
     * bit 2: File system uses 64 bit size field
     * bit 3: Directory contents are stored in the form of a Binary Tree 
    */
    uint32_t ro_features; // If these features are not supported, mount as RO

    uint8_t file_system_id[16];

    char volume_name[16]; // C style string
    char path_last_mounted[64]; // C style string

    uint32_t compression_algorithm_used;

    uint8_t file_block_preallocation_count;
    uint8_t dir_block_preallocation_count;

    uint16_t rsv;

    uint8_t journal_id[16];

    uint32_t journal_inode;
    uint32_t journal_device;

    uint32_t head_of_orphan_inode_list;

    uint8_t rsv2[1024 - 236];
} __attribute__((packed));

static_assert(sizeof(ext2_extended_superblock_t) == (1024 - sizeof(ext2_superblock_t)), "Invalid extended superblock size!");

struct ext2_block_group_descriptor_t{
    uint32_t block_of_block_usage_bitmap;
    uint32_t block_of_inode_usage_bitmap;

    uint32_t inode_table_block_address;

    uint16_t unallocated_block_count; // For this group
    uint16_t unallocated_inode_count;

    uint16_t directory_count;
    
    uint8_t rsv[14];
} __attribute__((packed));

static_assert(sizeof(ext2_block_group_descriptor_t) == 32, "Invalid block group descriptor size!");

struct ext2_inode_t{
    uint16_t type_permissions;
    uint16_t owner_uid;

    uint32_t size_low;

    uint32_t last_access_time;
    uint32_t change_time;
    uint32_t last_modification_time;
    uint32_t deletion_time;

    uint16_t owner_gid;

    uint16_t hard_link_count;
    uint32_t sectors_used; // sectors used by this inode
    uint32_t flags;

    uint32_t rsv;

    uint32_t direct_block_pointer[12];
    uint32_t single_indirect_block_pointer; // Points to a block that is a list of block pointers to data
    uint32_t double_indirect_block_pointer; // Points to a block that is a list of single_indirect_block_pointers
    uint32_t triple_indirect_block_pointer; // Points to a block that is a list of double_indirect_block_pointers

    uint32_t generation_number;

    // Only for major_verion >= 1
    uint32_t extended_attribute_block;
    uint32_t size_upper;

    uint32_t fragment_block_address;

    uint32_t os_specific;
} __attribute__((packed));

struct ext2_directory_entry_t{
    uint32_t inode;
    uint16_t entry_size;
    uint8_t name_size_low;
    uint8_t type; // Type indicator (only if the feature bit for "directory entries have file type byte" is set, else this is the most-significant 8 bits of the Name Length) 
    char name[1];
} __attribute__((packed));