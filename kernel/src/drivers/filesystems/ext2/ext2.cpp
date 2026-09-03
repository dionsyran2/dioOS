#include <drivers/filesystems/ext2/ext2.h>
#include <drivers/filesystems/common.h>
#include <vfs/vfs.h>
#include <drivers/drivers.h>
#include <kstdio.h>
#include <drivers/timers/common.h>
#include <memory.h>
#include <memory/heap.h>
#include <cstr.h>

filesystem_t *ext2_create_instance(vnode_t *disk){
    disk->open();
    return new filesystems::ext2_t(disk);
}

bool ext2_valid_fs(vnode_t *disk){
    ext2_superblock_t sblock;
    if (disk->read(&sblock, sizeof(sblock), EXT2_SUPERBLOCK_OFFSET) <= 0) {
        return false;
    }

    bool is_ext2 = sblock.signature == EXT2_SIGNATURE;

    return is_ext2;
}

void ext2_drv_init(){
    filesystem_registration_t * reg = new filesystem_registration_t();
    reg->create_fs = ext2_create_instance;
    reg->has_valid_fs = ext2_valid_fs;

    register_filesystem(reg);
}

DEFINE_DEVICELESS_DRIVER(ext2_driver) = {
    .name = "EXT2 Filesystem",
    .initialize = ext2_drv_init
};

namespace filesystems {
    ext2_t::ext2_t(vnode_t *disk) : filesystem_t(disk) {
        this->disk = disk;
    };

    ext2_t::~ext2_t(){
        
    };

    extern vnode_t *ext2_fs_fetch_vnode(dentry_t *entry);

    dentry_t *ext2_t::mount(){
        this->filesystem_id = vfs::allocate_filesystem_id();
        this->_initialize();

        dentry_t *ret = new dentry_t();
        strcpy(ret->name, "ext2_root_node");

        ret->fs_data = this;
        ret->fs_id = this->filesystem_id;
        ret->inode = EXT2_ROOT_INODE;
        ret->__fs_fetch_vnode = ext2_fs_fetch_vnode;
        return ret;
    }

    void ext2_t::umount(){
        if (this->block_group_table) {
            free(this->block_group_table);
        }
    }

    bool ext2_t::_initialize(){
        if (!this->_load_superblock()) {
            kprintf("[EXT2] Could not load the superblock\n");
            return false;
        }

        if (this->superblock.major_version < 1) {
            kprintf("[EXT2] Version %d.%d not supported!\n",
                this->superblock.major_version, this->superblock.minor_version);
            return false;
        }

        if (!this->_load_extended_superblock()) {
            kprintf("[EXT2] Could not load the extended superblock\n");
            return false;
        }

        // Check required features
        if (this->extended_superblock.required_features & 0b101){
            kprintf("[EXT2] Required features are not supported!\n");
            return false;
        }

        // Calculate the block size
        this->block_size = 1024 << this->superblock.log2_block_size;

        if (!this->_load_block_group_table()) {
            kprintf("[EXT2] Could not load the block group table\n");
            return false;
        }

        // Update the last mount time & times mounted
        this->superblock.last_mount_time = current_time;
        this->superblock.mount_count++;

        // Write changes to the superblock
        _save_superblock();

        // Update size informaton
        this->partition_total_size = _get_total_size();
        this->used_space = _calculate_used_space();

        return true;
    }

    bool ext2_t::_load_superblock(){
        return this->disk->read(&this->superblock, sizeof(ext2_superblock_t), EXT2_SUPERBLOCK_OFFSET) == sizeof(ext2_superblock_t);
    }

    bool ext2_t::_save_superblock(){
        return this->disk->write(&this->superblock, sizeof(ext2_superblock_t), EXT2_SUPERBLOCK_OFFSET) == sizeof(ext2_superblock_t);
    }

    bool ext2_t::_load_extended_superblock(){
        return this->disk->read(&this->extended_superblock, sizeof(ext2_extended_superblock_t), EXT2_SUPERBLOCK_OFFSET + sizeof(ext2_superblock_t))
            == sizeof(ext2_extended_superblock_t);
    }

    bool ext2_t::_save_extended_superblock(){
        return this->disk->write(&this->extended_superblock, sizeof(ext2_extended_superblock_t), EXT2_SUPERBLOCK_OFFSET + sizeof(ext2_superblock_t))
            == sizeof(ext2_extended_superblock_t);
    }
    
     bool ext2_t::_load_block_group_table(){
        // Calculate the total block_group_count
        this->block_group_count = ROUND_UP(this->superblock.total_block_count, this->superblock.blocks_per_block_group)
                / this->superblock.blocks_per_block_group;

        uint64_t block_group_size = sizeof(ext2_block_group_descriptor_t) * this->block_group_count;

        if (this->block_group_table == nullptr){
            this->block_group_table = (ext2_block_group_descriptor_t*)malloc(block_group_size);
        }

        uint32_t block = block_size == 1024 ? 2 : 1;

        return this->disk->read(this->block_group_table, block_group_size, _block_to_offset(block)) == block_group_size;
    }

    bool ext2_t::_save_block_group_table(){
        uint64_t block_group_size = sizeof(ext2_block_group_descriptor_t) * this->block_group_count;
        uint32_t block = block_size == 1024 ? 2 : 1;

        return this->disk->write(this->block_group_table, block_group_size, _block_to_offset(block)) == block_group_size;
    }

    uint64_t ext2_t::_get_total_size() {
        uint64_t total_blocks = this->superblock.total_block_count;

        return total_blocks * this->block_size;
    }

    uint64_t ext2_t::_calculate_used_space(){
        uint32_t total_blocks = this->superblock.total_block_count;
        uint32_t free_blocks = this->superblock.unallocated_block_count;

        uint64_t used_blocks = (uint64_t)(total_blocks - free_blocks);

        return used_blocks * this->block_size;
    }
};