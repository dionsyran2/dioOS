#pragma once
#include <stdint.h>
#include <stddef.h>
#include <filesystem/vfs/vfs.h>
#include <filesystem/ext2/ext2_structures.h>
#include <filesystem/ext2/ext2_defs.h>

namespace filesystems{
    bool is_ext2(vnode_t* blk);

    class ext2_t{
        public:
        ext2_t(vnode_t* blk);
        ~ext2_t();

        // @brief Prepares the filesystem to be mounted
        bool mount();
        
        // @brief Cleans up any used memory and syncs changes. The root_dir vnode should be removed by the vfs
        bool unmount();

        // Create a root node for this fs
        vnode_t* create_root_node();

        // Inode Stuff
        ext2_inode_t* fetch_inode(uint32_t inode);
        void update_inode(uint32_t inode, ext2_inode_t* inode_struct);
        int64_t load_inode_contents(uint32_t inode, uint64_t offset, uint64_t size, void* buffer);
        int64_t write_inode_contents(uint32_t inode_num, uint64_t offset, uint64_t size, const void* buffer);
        uint64_t get_inode_size(ext2_inode_t* inode);
        int truncate_inode(uint32_t inode_num, uint64_t new_size);

        // Directories
        int create_directory(uint32_t parent_inode_num, const char* name);
        int directory_add_entry(uint32_t dir_inode_num, uint32_t new_inode_num, const char* name, uint16_t type);
        int directory_remove_entry(uint32_t dir_inode_num, const char* name);
        int free_inode_contents(uint32_t inode);
        int create_file(uint32_t parent_inode_num, const char* name);
        private:
        bool _load_superblock();
        bool _save_superblock();

        bool _load_extended_superblock();
        bool _save_extended_superblock();
        
        bool _load_block_group_table();
        bool _save_block_group_table();

        uint64_t _block_to_offset(uint32_t block);

        uint64_t _get_total_size();
        uint64_t _calculate_used_space();

        int64_t _allocate_block(uint32_t start_group);
        int64_t _allocate_blocks(uint32_t start_group, uint32_t requested_count, uint32_t* out_allocated_count);

        void _free_block(uint32_t block);

        int64_t _allocate_inode(uint32_t start_group);
        void _free_inode(uint32_t inode);

        // Block fetching
        uint32_t _read_indirect_entry(uint32_t block_id, uint32_t index);
        uint32_t _get_physical_block_l1(uint32_t l1_table, uint32_t logical);
        uint32_t _get_physical_block_l2(uint32_t l2_table, uint32_t logical_l2, uint32_t logical_l1);
        uint32_t _get_physical_block_l3(uint32_t l3_table, uint32_t logical_l3, uint32_t logical_l2, uint32_t logical_l1);
        uint32_t _get_physical_block(ext2_inode_t* inode, uint64_t logical_block);

        // Block Editing
        uint32_t _create_indirect_block(uint32_t parent_group);
        bool _set_indirect_entry(uint32_t block_id, uint32_t index, uint32_t value);

        bool _set_physical_block_l1(uint32_t l1_table, uint32_t logical, uint32_t value);
        bool _set_physical_block_l2(uint32_t l2_table, uint32_t logical_l2, uint32_t logical_l1, uint32_t value);
        bool _set_physical_block_l3(uint32_t l3_table, uint32_t logical_l3, uint32_t logical_l2, uint32_t logical_l1, uint32_t value);
        bool _set_physical_block(ext2_inode_t* inode, uint32_t logical_block, uint32_t value);

        // Block Freeing
        void _free_triple_indirect(uint32_t triple_indirect_block);
        void _free_double_indirect(uint32_t double_indirect_block);
        void _free_single_indirect(uint64_t single_indirect_block);
        
        public:
        
        private:

        vnode_t* blk;
        vnode_t* root;
        bool mounted;
        bool read_only;
        int ref_count;

        // FS Data
        uint64_t block_size;
        uint64_t fragment_size;

        spinlock_t block_group_table_lock;
        uint64_t block_group_count;

        ext2_superblock_t* superblock;
        ext2_extended_superblock_t* extended_superblock;
        ext2_block_group_descriptor_t* block_group_table;
    };
}