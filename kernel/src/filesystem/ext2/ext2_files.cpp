#include <filesystem/ext2/ext2.h>
#include <memory/heap.h>
#include <kstdio.h>
#include <drivers/timers/common.h>
#include <cstr.h>
#include <math.h>
#include <kerrno.h>
#include <scheduling/task_scheduler/task_scheduler.h>

namespace filesystems{
    int ext2_t::create_file(uint32_t parent_inode_num, const char* name) {
        // Allocate a new Inode
        uint32_t parent_group = (parent_inode_num - 1) / this->superblock->inodes_per_block_group;
        int64_t new_inode_num = _allocate_inode(parent_group);
        if (new_inode_num < 0) return -ENOSPC;

        // Initialize the Inode Structure
        ext2_inode_t* new_inode = new ext2_inode_t;
        memset(new_inode, 0, sizeof(ext2_inode_t));
        
        new_inode->type_permissions = EXT2_INODE_REG | 0755;
        new_inode->hard_link_count = 1;
        new_inode->change_time = current_time;
        new_inode->last_access_time = current_time;
        new_inode->last_modification_time = current_time;
        new_inode->sectors_used = 0;
        new_inode->size_low = 0;

        // Permissions
        task_t* self = task_scheduler::get_current_task();
        if (self){
            new_inode->owner_uid = self->euid;
            new_inode->owner_gid = self->egid;
        }

        // Save the inode so it "exists" before we write data to it
        update_inode(new_inode_num, new_inode);

        
        // Preallocate the mandated amount
        if (this->extended_superblock->dir_block_preallocation_count){
            truncate_inode(new_inode_num, this->extended_superblock->dir_block_preallocation_count * this->block_size);
        }

        delete new_inode;

        // Link into Parent
        directory_add_entry(parent_inode_num, new_inode_num, name, EXT2_INODE_REG);

        return 0;
    }
}