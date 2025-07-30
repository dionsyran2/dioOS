#pragma once
#include <stdint.h>
#include <stddef.h>
#include <VFS/vfs.h>
#include <drivers/rtc/rtc.h>
#include <drivers/storage/gpt/gpt.h>

#define EXT2_SUPERBLOCK_OFFSET 1024
#define EXT2_SUPERBLOCK_SIZE 1024


struct ext2_extended_superblock{
    uint32_t first_non_reserved_inode;
    uint16_t size_of_inode;
    uint16_t this_block_group;
    uint32_t optional_features;
    uint32_t required_features;
    uint32_t features_ro; // mount RO if not supported
    uint64_t file_system_id[2];
    char last_mount_path[64];
    uint32_t compression_used;
    uint8_t blocks_to_preallocate; // Number of blocks to preallocate for files
    uint8_t blocks_to_preallocate_dir; // Number of blocks to preallocate for directories
    uint16_t unused;
    uint64_t journal_id[2]; // Journal id
    uint32_t journal_inode;
    uint32_t journal_device;
    uint32_t head_of_orphan_inode;
    // UNUSED
    // ...
} __attribute__((packed));

struct ext2_superblock{
    uint32_t num_of_inodes; // Total number of inodes in the file system
    uint32_t num_of_blocks;  // Total number of blocks in the file system
    uint32_t num_of_blocks_rsv_superuser;
    uint32_t num_of_unallocated_blocks;
    uint32_t num_of_unallocated_inodes;
    uint32_t block_number_of_superblock;
    uint32_t block_size; // log2 (block size) - 10. (In other words, the number to shift 1,024 to the left by to obtain the block size)
    uint32_t fragment_size; // log2 (fragment size) - 10. (In other words, the number to shift 1,024 to the left by to obtain the fragment size)
    uint32_t num_of_blocks_in_group;
    uint32_t num_of_fragments_in_group;
    uint32_t num_of_inodes_in_group;
    uint32_t last_mount_time;
    uint32_t last_written_time;
    uint16_t num_of_times_mounted; // Number of times the volume has been mounted since its last consistency check
    uint16_t num_of_mounts_allowed; // Number of mounts allowed before a consistency check (fsck) must be done  
    uint16_t signature;
    uint16_t file_system_state;
    uint16_t error_detected; // ????
    uint16_t minor_version;
    uint32_t last_check_time;
    uint32_t interval_check_time;
    uint32_t os_id;
    uint32_t major_version;
    uint16_t user_id_rsv_blocks;
    uint16_t group_id_rsv_blocks;

    ext2_extended_superblock extended; //only present if Major version, is greater than or equal to 1. 
} __attribute__((packed));

struct ext2_block_group_descriptor{
    uint32_t block_usage_bitmap;
    uint32_t inode_usage_bitmap;
    uint32_t inode_table_block;
    uint16_t unallocated_block_count;
    uint16_t unallocated_inode_count;
    uint16_t directory_count;
    uint8_t padding[14];
} __attribute__((packed));

#define EXT2_FIFO   0x1000
#define EXT2_CHR    0x2000
#define EXT2_DIR    0x4000
#define EXT2_BLK    0x6000
#define EXT2_REG    0x8000
#define EXT2_SYM    0xA000
#define EXT2_SOC    0xC000

#define EXT2_OX     0x001
#define EXT2_OW     0x002
#define EXT2_OR     0x004
#define EXT2_GX     0x008
#define EXT2_GW     0x010
#define EXT2_GR     0x020
#define EXT2_UX     0x040
#define EXT2_UW     0x080
#define EXT2_UR     0x100
/*
*   For files, particularly executables, the superuser could tag these as to be retained in main memory,
*   even when their need ends, to minimize swapping that would occur when another need arises, and the
*   file now has to be reloaded from relatively slow secondary memory.[1] This function has become
*   obsolete due to swapping optimization.

*   For directories, when a directory's sticky bit is set, the filesystem treats the files in such directories
*   in a special way so only the file's owner, the directory's owner, or root user can rename or delete the file.
*   Without the sticky bit set, any user with write and execute permissions for the directory can rename or delete
*   contained files, regardless of the file's owner. Typically this is set on the /tmp directory to prevent ordinary
*   users from deleting or moving other users' files.

*   The modern function of the sticky bit refers to directories, and protects directories and their content from being
*   hijacked by non-owners; this is found in most modern Unix-like systems. Files in a shared directory such as /tmp belong
*   to individual owners, and non-owners may not delete, overwrite or rename them. 
*/
#define EXT2_STICKY 0x200 // sticky bit?
#define EXT_SETGID  0x400
#define EXT_SETUID  0x800

// inode flags
#define EXT2_SECURE_DELETION    0x0000001 // not used
#define EXT2_KEEP_COPY_OF_DATA  0x0000002 // not used
#define EXT2_FILE_COMPRESSION   0x0000004 // not used
#define EXT2_SYNC_UPDATES       0x0000008 // changes are written immidiatelly to the disk
#define EXT2_IMMUTABLE_FILE     0x0000010 // Immutable file (content cannot be changed) | --- so read-only?
#define EXT2_APPEND_ONLY        0x0000020 // sounds crazy, so you cannot erase data, only add to it?
#define EXT2_FILE_NOT_INCLUDED  0x0000040 // file not included on dump command
#define EXT2_DONT_UPDATE_LA     0x0000080 // Don't update last accessed time
#define EXT2_HASH_INDEXED_DIR   0x0010000 // hash indexed directory
#define EXT2_AFS_DIRECTORY      0x0020000 // AFS Directory
#define EXT2_JOURNAL_FILE_DATA  0x0040000 // fuck you... aint no way i am gonna do that

struct ext2_ossv2_linux{
    uint8_t fragment_number;
    uint8_t fragment_size;
    uint16_t rsvd;
    uint16_t user_id_high;
    uint16_t group_id_high;
    uint32_t rsvd2;
} __attribute__((packed));

struct ext2_inode{
    uint16_t type_perms; // type & perms
    uint16_t user_id;
    uint32_t size_low;
    uint32_t last_access_time;
    uint32_t creation_time;
    uint32_t last_modification_time;
    uint32_t deletion_time;
    uint16_t group_id;
    uint16_t count_of_hard_links; // when this reaches 0, the data blocks are marked as unallocated
    uint32_t count_of_disk_sectors; // Count of disk sectors (not Ext2 blocks) in use by this inode, not counting the actual inode structure nor directory entries linking to the inode. 
    uint32_t flags;
    uint32_t ossv1;
    uint32_t direct_block_pointer[12]; // an array of direct block pointers.
    uint32_t singly_indirect_block_pointer; // Singly Indirect Block Pointer (Points to a block that is a list of block pointers to data) 
    uint32_t doubly_indirect_block_pointer; // Doubly Indirect Block Pointer (Points to a block that is a list of block pointers to Singly Indirect Blocks)
    uint32_t triply_indirect_block_pointer; // Triply Indirect Block Pointer (Points to a block that is a list of block pointers to Doubly Indirect Blocks)
    uint32_t generation_number;
    uint32_t extended_attribute_block; // in version 0 rsvd
    uint32_t size_upper; // in version 0 rsvd
    uint32_t fragment_block;
    ext2_ossv2_linux ossv2;
} __attribute__((packed));


/*
*   Directories are inodes which contain some number of "entries" as their contents.
*   These entries are nothing more than a name/inode pair. For instance the inode
*   corresponding to the root directory might have an entry with the name of "etc"
*   and an inode value of 50. A directory inode stores these entries in a linked-list
*   fashion in its contents blocks.

*   The root directory is Inode 2.

*   The total size of a directory entry may be longer then the length of the name would imply
*   (The name may not span to the end of the record), and records have to be aligned to 4-byte
*   boundaries. Directory entries are also not allowed to span multiple blocks on the file-system,
*   so there may be empty space in-between directory entries. Empty space is however not allowed
*   in-between directory entries, so any possible empty space will be used as part of the preceding
*   record by increasing its record length to include the empty space. Empty space may also be equivalently
*   marked by a separate directory entry with an inode number of zero, indicating that directory entry should be skipped. 
*/

#define EXT2_DIRENT_UNKNOWN     0
#define EXT2_DIRENT_REG         1
#define EXT2_DIRENT_DIR         2
#define EXT2_DIRENT_CHR         3
#define EXT2_DIRENT_BLK         4
#define EXT2_DIRENT_FIFO        5
#define EXT2_DIRENT_SOCK        6
#define EXT2_DIRENT_SYM         7 // Symbolic link (soft link) 


struct ext2_directory{
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_length;
    uint8_t type_or_name; // (only if the feature bit for "directory entries have file type byte" is set, else this is the most-significant 8 bits of the Name Length) 
    char name[];
};

namespace filesystem{
    bool is_ext2(vblk_t* blk, uint64_t start_block);

    class ext2{
        public:
        ext2(vblk_t* blk, uint64_t start_block, uint64_t last_block, PartEntry* gpt_entry);
        ext2_inode* read_inode(uint32_t inode);
        void* load_inode(ext2_inode* inode, uint32_t* length);
        bool write_inode(uint32_t inode, void* buff, uint32_t ln);
        int mkdir(uint32_t parent_inode, const char* name, ext2_directory** out);
        int create_file(uint32_t parent_inode, const char* name, ext2_directory** out);
        vnode_t* create_and_mount_dir(ext2_directory* dir, ext2_inode* inode, vnode_t* parent);
        void mount_dir(uint32_t inode, vnode_t* parent, bool sub);
        private:
        vblk_t* blk;
        
        uint64_t start_sector;
        uint64_t last_sector;
        uint8_t partition_guid[16];

        uint32_t block_size;
        uint32_t sectors_per_block;

        uint64_t superblock_size_in_pages;
        uint64_t superblock_size_in_sectors;
        uint64_t superblock_size_in_bytes;
        uint64_t superblock_sector;
    
        uint64_t block_group_count;
        uint64_t block_group_table_sector;
        uint64_t block_group_table_size_in_pages;
        uint64_t block_group_table_size_in_sectors;
        uint64_t block_group_table_bytes;

        uint32_t inode_size_in_bytes;
        uint32_t inodes_per_group;

        ext2_superblock* superblock;
        ext2_block_group_descriptor* block_group_table;

        private:
        void _load_superblock();
        void _save_superblock();
        void _parse_superblock();
        void _load_block_group_descriptor();
        void _save_block_group_descriptor();
        uint32_t _number_of_blocks(ext2_inode* inode);
        uint32_t _number_of_blocks_direct(ext2_inode* inode);
        uint32_t _number_of_blocks_indirect_l1(uint32_t singly_indirect_block_pointer); // single indirect
        uint32_t _number_of_blocks_indirect_l2(uint32_t double_indirect_block_pointer); // double indirect
        uint32_t _number_of_blocks_indirect_l3(uint32_t triple_indirect_block_pointer);
        void _push_block_table_direct(ext2_inode* inode, uint32_t* table, uint32_t* offset);
        void _push_block_table_indirect_l1(uint32_t singly_indirect_block_pointer, uint32_t* table, uint32_t* offset);
        void _push_block_table_indirect_l2(uint32_t double_indirect_block_pointer, uint32_t* table, uint32_t* offset);
        void _push_block_table_indirect_l3(uint32_t triple_indirect_block_pointer, uint32_t* table, uint32_t* offset);
        uint32_t* _get_block_table(ext2_inode* inode, uint32_t* length);
        uint64_t _block_to_sector(uint64_t block);
        char* _mount_fs();
        bool _save_inode_entry(uint32_t inode, ext2_inode* inode_entry);
        uint32_t _find_free_inode();
        bool _mark_inode(uint32_t inode, bool v);
        uint32_t _find_free_block();
        bool _mark_block(uint32_t block, bool v);
        void _clear_block(uint32_t block);
        void _inode_allocate_singly_indirect_block(ext2_inode* inode);
        void _inode_allocate_double_indirect_block(ext2_inode* inode);
        void _inode_allocate_triple_indirect_block(ext2_inode* inode);
        bool _inode_add_block_indirect_l1(uint32_t singly_indirect_block_pointer, uint32_t block);
        bool _inode_add_block_indirect_l2(uint32_t doubly_indirect_block_pointer, uint32_t block);
        bool _inode_add_block_indirect_l3(uint32_t triple_indirect_block_pointer, uint32_t block);
        void _inode_add_block(ext2_inode* inode, uint32_t block);
        size_t _inode_truncate_indirect_l1(uint32_t singly_indirect_block_pointer, size_t block_count);
        size_t _inode_truncate_indirect_l2(uint32_t doubly_indirect_block_pointer, size_t block_count);
        size_t _inode_truncate_indirect_l3(uint32_t triple_indirect_block_pointer, size_t block_count);
        uint32_t _inode_truncate(ext2_inode* inode, size_t num_of_blocks);
        bool _push_dir_entry(uint32_t parent, ext2_directory* directory_entry);
    };
}