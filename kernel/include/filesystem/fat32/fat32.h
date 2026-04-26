#pragma once
#include <stdint.h>
#include <stddef.h>
#include <filesystem/vfs/vfs.h>
#include <filesystem/fat32/fat32_structures.h>

#define FAT32_IDENTIFIER        "FAT32   " // Spec says to never trust it... oh well, what could go wrong
#define FAT32_END_OF_CHAIN           0x0FFFFFF8
#define FAT32_IS_END_OF_CHAIN(value) (value >= FAT32_END_OF_CHAIN)
#define FAT32_IS_CLUSTER_FREE(value) (value == 0)


struct vnode_t;

// The offset where the signature (0x28 or 0x29) is placed
namespace filesystems {
    bool is_fat32(vnode_t* blk);

    class fat32_t{
        public:
        fat32_t(vnode_t* blk);
        ~fat32_t();

        // @brief Creates a root node for this filesystem
        vnode_t* create_root_node();
        
        // @brief It will calculate the total size based on the size of the cluster chain
        uint64_t get_total_size(uint32_t start_cluster);

        // @brief Returns the cluster of the root directory
        uint32_t get_root_dir_cluster();

        // @brief Converts a cluster number to an offset relative to the start of the volume
        uint64_t get_cluster_offset(uint32_t cluster);

        // @brief Reads file data
        int read_contents(uint64_t start_cluster, uint64_t file_offset, uint64_t byte_length, void* buffer);
        int write_contents(uint64_t start_cluster, uint64_t file_offset, uint64_t byte_length, const void* buffer);

        uint32_t create_dir(uint32_t parent_cluster, char* name);
        int create_file(uint32_t parent_cluster, char* name, bool preallocate = true);

        bool load_directory_entry(uint32_t parent_cluster, char* name, fat_directory_entry_t* entry);
        bool update_directory_entry(uint32_t parent_cluster, char* name, fat_directory_entry_t* entry);

        void free_cluster_chain(uint32_t first_cluster);

        bool is_dir_empty(uint32_t cluster);
        private:
        bool _load_bpb();
        bool _load_ebr();
        bool _load_fsinfo();
        bool _load_fat();

        bool _save_bpb();
        bool _save_ebr();
        bool _save_fsinfo();
        bool _save_fat();


        bool _zero_cluster(uint32_t cluster);

        uint32_t _get_chained_clusters_count(uint32_t cluster); // Returns the length of the cluster chain
        uint32_t _read_fat_entry(uint32_t cluster);
        bool _write_fat_entry(uint32_t cluster, uint32_t value);

        uint32_t _find_free_cluster();
        int64_t _push_dir_entry(uint32_t parent_cluster, fat_directory_entry_t* entry);
        int _link_entry(uint32_t parent_cluster, const char* name, uint32_t target_cluster, uint8_t attributes);
        
        void _calculate_space();
        public:
        int dev_id;
        vnode_t* blk;
        vnode_t* root_node;

        private:
        fat_bpb_t bios_parameter_block;
        fat32_extended_boot_record_t extended_boot_record;
        fat32_fsinfo fsinfo;


        uint32_t* fat_table;

        uint64_t last_free_cluster;

    };

} // namespace fat32

