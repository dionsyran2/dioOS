#include <filesystem/ext2/ext2.h>
#include <memory/heap.h>
#include <kstdio.h>
#include <drivers/timers/common.h>
#include <cstr.h>
#include <math.h>
#include <kerrno.h>
#include <scheduling/task_scheduler/task_scheduler.h>

namespace filesystems{
    int ext2_t::directory_add_entry(uint32_t dir_inode_num, uint32_t new_inode_num, const char* name, uint16_t type) {
        ext2_inode_t* dir_inode = fetch_inode(dir_inode_num);
        int name_len = strlen(name);
        
        // Required space: 8 byte header + name_len -> Rounded up to 4 bytes
        uint16_t required_size = (8 + name_len + 3) & ~3;

        uint8_t* block_buf = (uint8_t*)malloc(this->block_size);
        uint64_t offset = 0;
        uint64_t dir_size = get_inode_size(dir_inode); // helper to get 64-bit size
        
        bool saved = false;

        // Iterate through the directory block by block
        while (offset < dir_size) {
            // Use your read implementation
            load_inode_contents(dir_inode_num, offset, this->block_size, block_buf);

            ext2_directory_entry_t* entry = (ext2_directory_entry_t*)block_buf;
            uint32_t internal_offset = 0;

            while (internal_offset < this->block_size) {
                // Calculate actual used size of this entry
                uint16_t real_used_size = (8 + entry->name_size_low + 3) & ~3;

                // Check if this is the last entry in the list and if it has slack space
                if (entry->entry_size != real_used_size && internal_offset + entry->entry_size >= this->block_size) {
                    
                    uint16_t free_space = entry->entry_size - real_used_size;

                    if (free_space >= required_size) {
                        // --- FOUND A GAP: SPLIT ENTRY ---
                        
                        // Shrink the current entry
                        uint16_t original_len = entry->entry_size;
                        entry->entry_size = real_used_size;

                        // Create new entry immediately after
                        ext2_directory_entry_t* new_entry = (ext2_directory_entry_t*)((uint8_t*)entry + real_used_size);
                        new_entry->inode = new_inode_num;
                        new_entry->entry_size = original_len - real_used_size; // Claim the rest
                        new_entry->name_size_low = name_len;
                        if (this->extended_superblock->required_features & EXT2_REQ_FEATURE_DIR_TYPE) new_entry->type = (type == EXT2_INODE_DIR) ? 2 : 1;
                        memcpy(new_entry->name, name, name_len);

                        // Write back ONLY this block
                        write_inode_contents(dir_inode_num, offset, this->block_size, block_buf);
                        
                        saved = true;
                        goto cleanup;
                    }
                }
                
                internal_offset += entry->entry_size;
                entry = (ext2_directory_entry_t*)(block_buf + internal_offset);
            }
            offset += this->block_size;
        }

        if (!saved) {
            memset(block_buf, 0, this->block_size);
            
            ext2_directory_entry_t* new_entry = (ext2_directory_entry_t*)block_buf;
            new_entry->inode = new_inode_num;
            new_entry->entry_size = this->block_size; // Takes full new block
            new_entry->name_size_low = name_len;
            if (this->extended_superblock->required_features & EXT2_REQ_FEATURE_DIR_TYPE) new_entry->type = (type == EXT2_INODE_DIR) ? 2 : 1;
            memcpy(new_entry->name, name, name_len);

            // Simply write at the end of the file. 
            write_inode_contents(dir_inode_num, dir_size, this->block_size, block_buf);
        }

    cleanup:
        free(block_buf);
        delete dir_inode;
        return 0;
    }

    int ext2_t::directory_remove_entry(uint32_t dir_inode_num, const char* name){
        uint64_t chunk_size = 4096;
        uint8_t* buffer = (uint8_t*)malloc(chunk_size);
        uint64_t file_offset = 0;
        size_t name_len = strlen(name);

        while (true) {
            int64_t bytes_read = load_inode_contents(dir_inode_num, file_offset, chunk_size, buffer);
            if (bytes_read <= 0) break;

            uint64_t buffer_pos = 0;
            ext2_directory_entry_t* prev = nullptr;
            
            while (buffer_pos < bytes_read) {
                ext2_directory_entry_t* entry = (ext2_directory_entry_t*)(buffer + buffer_pos);

                // Safety Check
                if (entry->entry_size == 0) break; 

                // Check match (Using memcmp because entry->name is NOT null-terminated)
                if (entry->name_size_low == name_len && memcmp(entry->name, name, name_len) == 0){
                    if (prev) {
                        prev->entry_size += entry->entry_size;
                    } else {
                        entry->inode = 0;
                    }

                    write_inode_contents(dir_inode_num, file_offset, chunk_size, buffer);
                    free(buffer);
                    return 0;
                }

                // Advance
                buffer_pos += entry->entry_size;
                prev = entry;
            }

            file_offset += bytes_read;
        }

        free(buffer);
        return -ENOENT;
    }
    
    int ext2_t::create_directory(uint32_t parent_inode_num, const char* name) {
        // Allocate a new Inode
        uint32_t parent_group = (parent_inode_num - 1) / this->superblock->inodes_per_block_group;
        int64_t new_inode_num = _allocate_inode(parent_group);
        if (new_inode_num < 0) return -ENOSPC;

        // Initialize the Inode Structure
        ext2_inode_t* new_inode = new ext2_inode_t;
        memset(new_inode, 0, sizeof(ext2_inode_t));
        
        new_inode->type_permissions = EXT2_INODE_DIR | 0755; // Directory, rwxrwxrwx
        new_inode->hard_link_count = 2;
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

        // Create the '.' and '..' Data Block
        uint8_t* buffer = (uint8_t*)malloc(this->block_size);
        memset(buffer, 0, this->block_size);

        // --- Entry 1: "." ---
        ext2_directory_entry_t* dot = (ext2_directory_entry_t*)buffer;
        dot->inode = new_inode_num;
        dot->name_size_low = 1;
        if (this->extended_superblock->required_features & EXT2_REQ_FEATURE_DIR_TYPE) dot->type = 2; // Directory
        dot->name[0] = '.';
        dot->entry_size = 12; 

        // --- Entry 2: ".." ---
        ext2_directory_entry_t* dotdot = (ext2_directory_entry_t*)(buffer + 12);
        dotdot->inode = parent_inode_num;
        dotdot->name_size_low = 2;
        if (this->extended_superblock->required_features & EXT2_REQ_FEATURE_DIR_TYPE) dotdot->type = 2; // Directory
        dotdot->name[0] = '.';
        dotdot->name[1] = '.';
        dotdot->entry_size = this->block_size - 12;

        // Preallocate the mandated amount
        if (this->extended_superblock->dir_block_preallocation_count){
            truncate_inode(new_inode_num, this->extended_superblock->dir_block_preallocation_count * this->block_size);
        }

        write_inode_contents(new_inode_num, 0, this->block_size, buffer);
        
        free(buffer);
        delete new_inode;

        // Link into Parent
        directory_add_entry(parent_inode_num, new_inode_num, name, EXT2_INODE_DIR);

        // Update Parent Link Count
        // Parent gets a new link because ".." in the child points to it
        ext2_inode_t* parent = fetch_inode(parent_inode_num);
        parent->hard_link_count++;
        update_inode(parent_inode_num, parent);


        delete parent;
        return 0;
    }
}