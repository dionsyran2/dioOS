#pragma once
#include <stdint.h>
#include <stddef.h>
#include <drivers/filesystems/common.h>
#include <drivers/filesystems/ext2/structures.h>
#include <drivers/filesystems/ext2/definitions.h>
#include <scheduling/spinlock/spinlock.h>

struct vnode_t;
struct dentry_t;

namespace filesystems {
    class ext2_t: public filesystem_t {
        public:
        dentry_t *mount();
        void umount();

        ~ext2_t();
        ext2_t(vnode_t *disk);

        uint64_t get_inode_size(ext2_inode_t* inode);

        int64_t load_inode_contents(uint32_t inode_num, uint64_t offset, uint64_t size, void* buffer);
        int64_t write_inode_contents(uint32_t inode_num, uint64_t offset, uint64_t size, const void* buffer);

        int64_t truncate_inode(uint32_t inode_num, uint64_t new_size);
        int free_inode_contents(uint32_t inode);

        int create_file(uint32_t parent_inode_num, const char* name, uint16_t mode);
        int create_directory(uint32_t parent_inode_num, const char* name, uint16_t mode);
        int directory_add_entry(uint32_t dir_inode_num, uint32_t new_inode_num, const char* name, uint16_t type);
        int directory_remove_entry(uint32_t dir_inode_num, const char* name);
        
        ext2_inode_t *fetch_inode(uint32_t inode);
        void update_inode(uint32_t inode, ext2_inode_t* inode_struct);

        void ext2_convert_dentry(ext2_directory_entry_t *entry, dentry_t *vfs_entry);
        
        private:
        bool _initialize();

        bool _load_superblock();
        bool _save_superblock();

        bool _load_extended_superblock();
        bool _save_extended_superblock();
        
        bool _load_block_group_table();
        bool _save_block_group_table();

        uint64_t _calculate_used_space();
        uint64_t _get_total_size();
        uint64_t _block_to_offset(uint32_t block);

        int64_t _allocate_block(uint32_t start_group);
        int64_t _allocate_blocks(uint32_t start_group, uint32_t requested_count, uint32_t* out_allocated_count);
        void _free_block(uint32_t block);

        int64_t _allocate_inode(uint32_t start_group);
        void _free_inode(uint32_t inode);

        uint32_t _create_indirect_block(uint32_t parent_group);
        uint32_t _read_indirect_entry(uint32_t block_id, uint32_t index);
        bool _set_indirect_entry(uint32_t block_id, uint32_t index, uint32_t value);

        uint32_t _get_physical_block_l1(uint32_t l1_table, uint32_t logical);
        uint32_t _get_physical_block_l2(uint32_t l2_table, uint32_t logical_l2, uint32_t logical_l1);
        uint32_t _get_physical_block_l3(uint32_t l3_table, uint32_t logical_l3, uint32_t logical_l2, uint32_t logical_l1);
        uint32_t _get_physical_block(ext2_inode_t* inode, uint64_t logical_block);

        bool _set_physical_block_l1(uint32_t l1_table, uint32_t logical, uint32_t value);
        bool _set_physical_block_l2(uint32_t l2_table, uint32_t logical_l2, uint32_t logical_l1, uint32_t value);
        bool _set_physical_block_l3(uint32_t l3_table, uint32_t logical_l3, uint32_t logical_l2, uint32_t logical_l1, uint32_t value);
        bool _set_physical_block(ext2_inode_t* inode, uint32_t logical_block, uint32_t value);

        void _free_triple_indirect(uint32_t triple_indirect_block);
        void _free_double_indirect(uint32_t double_indirect_block);
        void _free_single_indirect(uint64_t single_indirect_block);

        private:
        // FS Data
        uint64_t block_size;
        uint64_t fragment_size;

        spinlock_t block_group_table_lock;
        uint64_t block_group_count;

        ext2_superblock_t superblock;
        ext2_extended_superblock_t extended_superblock;
        ext2_block_group_descriptor_t* block_group_table;

        int filesystem_id;


        size_t partition_total_size;
        size_t used_space;

        vnode_t *disk;
    };
};