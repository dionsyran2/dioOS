#pragma once
#include <stdint.h>
#include <stddef.h>
#include <filesystem/FAT32/fat32_common.h>
#include <VFS/vfs.h>
#include <drivers/rtc/rtc.h>

namespace filesystem{
    bool is_fat32(vblk_t* blk, uint64_t start_block);

    class fat32{
        public:
        fat32(vblk_t* blk, uint64_t start_block);

        private:
        vblk_t* disk;
        FAT_BPB* bpb; // bios parameter block
        FAT32_EBR* ebr; // extended boot record
        //FAT32_FSINFO* fsinfo; // FS INFO
        uint64_t blocks_per_page;
        uint64_t start_block;

        uint16_t fat_start_block;
        uint32_t fat_size_in_bytes;
        uint32_t fat_size_in_blocks;
        uint8_t* file_allocation_table;

        
        private:
        void _init();
        void _parse_data();
        void _load_fat(); // loads the fat of the disk
        void _save_fat(); // saves the fat to the disk
        uint32_t calculate_block(uint32_t cluster);
        uint32_t get_next_cluster(uint32_t cluster);
        uint32_t get_amount_of_chained_cluster(uint32_t start_cluster);
        char* _parse_name(FAT_DIR* directory, int i);
        uint32_t _find_empty_cluster();
        bool _is_cluster_free(uint32_t cluster);
        void _mark_cluster(uint32_t cluster, uint32_t data);
        bool _has_dir(FAT_DIR* dir, char* name, bool sfn);
        char* _conv_name_to_sfn(char* name, uint8_t suffix, uint8_t* winnt);
        uint16_t _convert_date(RTC::rtc_time_t* time);
        uint16_t _convert_time(RTC::rtc_time_t* time);
        bool _createlfns(FAT32_LFN** array, size_t* count, char* name);

        public:
        int _list_dir(FAT_DIR* dir, vnode_t** nodes, size_t* out_count);
        int _mount_dir(FAT_DIR* dir, vnode_t* parent);
        uint8_t* _load_dir(FAT_DIR* dir, size_t* cnt);
        int write_data(FAT_DIR* dir, FAT_DIR* parent, char* data, size_t cnt);
        FAT_DIR* _make_dir(FAT_DIR* parent, bool dir, char* name);
        vnode_t* _create_vnode_from_fat_dir(FAT_DIR* dir, char* name);


    };
}