#include <filesystem/ext2/ext2.h>
#include <memory/heap.h>
#include <kstdio.h>
#include <drivers/timers/common.h>
#include <cstr.h>
#include <math.h>
#include <kerrno.h>


namespace filesystems{
    int64_t ext2_t::_allocate_inode(uint32_t start_group) {
        uint64_t groups_count = this->block_group_count;
        void* bitmap = malloc(this->block_size);
        
        uint32_t original_start_group = start_group;
        uint32_t current_start = start_group;
        uint32_t current_end = groups_count;

        uint64_t rflags = spin_lock(&this->block_group_table_lock);

        for (int i = 0; i < 2; i++) {
            for (uint32_t indx = current_start; indx < current_end; indx++) {
                ext2_block_group_descriptor_t* group = &block_group_table[indx];
                
                if (group->unallocated_inode_count == 0) continue;

                // Read Bitmap
                if (blk->read(_block_to_offset(group->block_of_inode_usage_bitmap), this->block_size, bitmap) < 0) {
                    free(bitmap);
                    spin_unlock(&this->block_group_table_lock, rflags);
                    return -EIO;
                }

                uint64_t inodes_per_group = this->superblock->inodes_per_block_group;

                for (uint64_t b = 0; b < inodes_per_group; b++) {
                    uint32_t* bitmask = (uint32_t*)bitmap + (b / 32);
                    
                    if ((*bitmask & (1 << (b % 32))) == 0) {
                        // --- FOUND FREE INODE --- //

                        *bitmask |= (1 << (b % 32));

                        if (blk->write(_block_to_offset(group->block_of_inode_usage_bitmap), this->block_size, bitmap) < 0) {
                            free(bitmap);
                            spin_unlock(&this->block_group_table_lock, rflags);
                            return -EIO;
                        }

                        // Update Counts
                        group->unallocated_inode_count--;
                        this->superblock->unallocated_inode_count--;

                        _save_block_group_table(); 
                        _save_superblock();

                        free(bitmap);
                        spin_unlock(&this->block_group_table_lock, rflags);

                        return (indx * inodes_per_group) + b + 1;
                    }
                }
            }
            current_start = 0;
            current_end = original_start_group;
        }

        free(bitmap);
        spin_unlock(&this->block_group_table_lock, rflags);
        return -ENOSPC;
    }

    void ext2_t::_free_inode(uint32_t inode) {
        if (inode < this->extended_superblock->first_non_reserved_inode) return;

        uint64_t rflags = spin_lock(&this->block_group_table_lock);

        uint32_t real_index = inode - 1; 

        uint32_t block_group = real_index / this->superblock->inodes_per_block_group;
        uint32_t rel_inode   = real_index % this->superblock->inodes_per_block_group;

        if (block_group >= this->block_group_count) {
            spin_unlock(&this->block_group_table_lock, rflags);
            return;
        }

        ext2_block_group_descriptor_t* group = &block_group_table[block_group];
        
        void* bitmap = malloc(this->block_size);
        if (!blk->read(_block_to_offset(group->block_of_inode_usage_bitmap), this->block_size, bitmap)){
            free(bitmap);
            spin_unlock(&this->block_group_table_lock, rflags);
            return;
        }

        uint32_t* bitmask = (uint32_t*)bitmap + (rel_inode / 32);
        
        if (*bitmask & (1 << (rel_inode % 32))){
            
            *bitmask &= ~(1 << (rel_inode % 32));

            if (!blk->write(_block_to_offset(group->block_of_inode_usage_bitmap), this->block_size, bitmap)){
                free(bitmap);
                spin_unlock(&this->block_group_table_lock, rflags);
                return;
            }

            group->unallocated_inode_count++;
            this->superblock->unallocated_inode_count++;

            _save_block_group_table(); 
            _save_superblock();
        }

        free(bitmap);
        spin_unlock(&this->block_group_table_lock, rflags);
    }

    ext2_inode_t* ext2_t::fetch_inode(uint32_t inode) {
        // Determine Inode Size
        uint32_t inode_size = 128;
        if (this->superblock->major_version >= 1) {
            inode_size = this->extended_superblock->inode_size;
        }

        // Calculate Block Group and Index
        uint32_t block_group = (inode - 1) / this->superblock->inodes_per_block_group;
        uint32_t index       = (inode - 1) % this->superblock->inodes_per_block_group;

        // Calculate absolute byte offset in the Table
        uint32_t total_byte_offset  = index * inode_size;
        uint32_t containing_block   = total_byte_offset / this->block_size;
        uint32_t offset_in_block    = total_byte_offset % this->block_size;

        // Calculate Final Disk Address
        uint32_t final_block_addr = this->block_group_table[block_group].inode_table_block_address 
                                + containing_block;

        uint64_t offset = _block_to_offset(final_block_addr) + offset_in_block;

        // Read safely
        ext2_inode_t* node = new ext2_inode_t;
        
        // Don't read more than the struct can hold, 
        // and don't read more than exists on disk.
        uint32_t to_read = sizeof(ext2_inode_t);
        if (to_read > inode_size) to_read = inode_size;

        memset(node, 0, sizeof(ext2_inode_t));

        if (!blk->read(offset, to_read, node)) {
            delete node;
            return nullptr;
        }

        return node;
    }

    void ext2_t::update_inode(uint32_t inode, ext2_inode_t* inode_struct) {
        // Determine Inode Size
        uint32_t inode_size = 128;
        if (this->superblock->major_version >= 1) {
            inode_size = this->extended_superblock->inode_size;
        }

        // Calculate Block Group and Index
        uint32_t block_group = (inode - 1) / this->superblock->inodes_per_block_group;
        uint32_t index       = (inode - 1) % this->superblock->inodes_per_block_group;

        // Calculate absolute byte offset in the Table
        uint32_t total_byte_offset  = index * inode_size;
        uint32_t containing_block   = total_byte_offset / this->block_size;
        uint32_t offset_in_block    = total_byte_offset % this->block_size;

        // Calculate Final Disk Address
        uint32_t final_block_addr = this->block_group_table[block_group].inode_table_block_address 
                                + containing_block;

        uint64_t offset = _block_to_offset(final_block_addr) + offset_in_block;

        uint32_t to_write = sizeof(ext2_inode_t);
        if (to_write > inode_size) to_write = inode_size;

        inode_struct->change_time = current_time;

        blk->write(offset, to_write, inode_struct);
    }

    uint32_t ext2_t::_create_indirect_block(uint32_t parent_group) {
        int64_t block = _allocate_block(parent_group);
        if (block <= 0) return 0;
        
        uint8_t* zeros = (uint8_t*)malloc(this->block_size);
        memset(zeros, 0, this->block_size);

        blk->write(_block_to_offset(block), this->block_size, zeros);
        free(zeros);
        
        return (uint32_t)block;
    }

    uint32_t ext2_t::_read_indirect_entry(uint32_t block_id, uint32_t index){
        uint32_t* block = (uint32_t*)malloc(this->block_size);
        if (block == nullptr) return 0;

        uint64_t offset = _block_to_offset(block_id);
        
        bool ret = blk->read(offset, this->block_size, block); // TODO Handle Failure

        uint32_t result = block[index];

        free(block);
        return result;
    }

    bool ext2_t::_set_indirect_entry(uint32_t block_id, uint32_t index, uint32_t value){
        uint32_t* block = (uint32_t*)malloc(this->block_size);
        if (block == nullptr) return false;

        uint64_t offset = _block_to_offset(block_id);
        
        bool ret = blk->read(offset, this->block_size, block);

        block[index] = value;

        ret = blk->write(offset, this->block_size, block);
        free(block);

        return ret;
    }
    
    uint32_t ext2_t::_get_physical_block_l1(uint32_t l1_table, uint32_t logical){
        return _read_indirect_entry(l1_table, logical);
    }

    uint32_t ext2_t::_get_physical_block_l2(uint32_t l2_table, uint32_t logical_l2, uint32_t logical_l1){
        uint32_t result = _read_indirect_entry(l2_table, logical_l2);
        if (result == 0) return 0; // Unallocated (Sparse files are not yet supported)

        return _get_physical_block_l1(result, logical_l1);
    }

    uint32_t ext2_t::_get_physical_block_l3(uint32_t l3_table, uint32_t logical_l3, uint32_t logical_l2, uint32_t logical_l1){
        uint32_t result = _read_indirect_entry(l3_table, logical_l3);
        if (result == 0) return 0; // Unallocated (Sparse files are not yet supported)

        return _get_physical_block_l2(result, logical_l2, logical_l1);
    }

    uint32_t ext2_t::_get_physical_block(ext2_inode_t* inode, uint64_t logical_block) {
        
        // Case 1: Direct Block
        if (logical_block < 12) {
            return inode->direct_block_pointer[logical_block];
        }
        
        // Normalize
        logical_block -= 12;

        // Calculate pointers per block
        uint32_t p_per_block = this->block_size / sizeof(uint32_t);

        // Case 2: Singly Indirect
        if (logical_block < p_per_block) {
            if (inode->single_indirect_block_pointer == 0) return 0;
            return _get_physical_block_l1(inode->single_indirect_block_pointer, logical_block);
        }
        
        // Normalize again
        logical_block -= p_per_block;

        // Case 3: Doubly Indirect
        if (logical_block < (uint64_t)p_per_block * p_per_block) {
            
            // Calculate the two indices
            uint32_t first_index  = logical_block / p_per_block;
            uint32_t second_index = logical_block % p_per_block;
            
            if (inode->double_indirect_block_pointer == 0) return 0;
            return _get_physical_block_l2(inode->double_indirect_block_pointer, first_index, second_index);
        }

        // Normalize yet again
        logical_block -= (p_per_block * p_per_block);

        // Case 4: Triply Indirect
        uint32_t first_index  = logical_block / (p_per_block * p_per_block);
        uint32_t remainder    = logical_block % (p_per_block * p_per_block);
        uint32_t second_index = remainder / p_per_block;
        uint32_t third_index  = remainder % p_per_block;

        if (inode->triple_indirect_block_pointer == 0) return 0;
        return _get_physical_block_l3(inode->triple_indirect_block_pointer, first_index, second_index, third_index);
    }

    bool ext2_t::_set_physical_block_l1(uint32_t l1_table, uint32_t logical, uint32_t value){
        return _set_indirect_entry(l1_table, logical, value);
    }

    bool ext2_t::_set_physical_block_l2(uint32_t l2_table, uint32_t logical_l2, uint32_t logical_l1, uint32_t value){
        int64_t result = _read_indirect_entry(l2_table, logical_l2);
        if (result == 0) {
            uint32_t blk_grp = l2_table / this->superblock->blocks_per_block_group;
            result = _create_indirect_block(blk_grp);
            if (result == 0 || result == -ENOSPC) return false;

            _set_indirect_entry(l2_table, logical_l2, result);
        }

        return _set_physical_block_l1(result, logical_l1, value);
    }

    bool ext2_t::_set_physical_block_l3(uint32_t l3_table, uint32_t logical_l3, uint32_t logical_l2, uint32_t logical_l1, uint32_t value){
        int64_t result = _read_indirect_entry(l3_table, logical_l3);
        if (result == 0) {
            uint32_t blk_grp = l3_table / this->superblock->blocks_per_block_group;
            result = _create_indirect_block(blk_grp);
            if (result == 0 || result == -ENOSPC) return false;

            _set_indirect_entry(l3_table, logical_l3, result);
        }

        return _set_physical_block_l2(result, logical_l2, logical_l1, value);
    }

    bool ext2_t::_set_physical_block(ext2_inode_t* inode, uint32_t logical_block, uint32_t value){
        // Case 1: Direct Block
        if (logical_block < 12) {
            inode->direct_block_pointer[logical_block] = value;
            return true;
        }
        
        // Normalize
        logical_block -= 12;

        // Calculate pointers per block
        uint32_t p_per_block = this->block_size / sizeof(uint32_t);

        // Case 2: Singly Indirect
        if (logical_block < p_per_block) {
            if (inode->single_indirect_block_pointer == 0) inode->single_indirect_block_pointer = _create_indirect_block(0);
            return _set_physical_block_l1(inode->single_indirect_block_pointer, logical_block, value);
        }
        
        // Normalize again
        logical_block -= p_per_block;

        // Case 3: Doubly Indirect
        if (logical_block < (uint64_t)p_per_block * p_per_block) {
            
            // Calculate the two indices
            uint32_t first_index  = logical_block / p_per_block;
            uint32_t second_index = logical_block % p_per_block;
            
            if (inode->double_indirect_block_pointer == 0) 
                inode->double_indirect_block_pointer = _create_indirect_block(inode->single_indirect_block_pointer / this->superblock->blocks_per_block_group);
            return _set_physical_block_l2(inode->double_indirect_block_pointer, first_index, second_index, value);
        }

        // Normalize yet again
        logical_block -= (p_per_block * p_per_block);

        // Case 4: Triply Indirect
        uint32_t first_index  = logical_block / (p_per_block * p_per_block);
        uint32_t remainder    = logical_block % (p_per_block * p_per_block);
        uint32_t second_index = remainder / p_per_block;
        uint32_t third_index  = remainder % p_per_block;

        if (inode->triple_indirect_block_pointer == 0) 
            inode->triple_indirect_block_pointer = _create_indirect_block(inode->double_indirect_block_pointer / this->superblock->blocks_per_block_group);
        return _set_physical_block_l3(inode->triple_indirect_block_pointer, first_index, second_index, third_index, value);
    }

    void ext2_t::_free_triple_indirect(uint32_t triple_indirect_block){
        if (!triple_indirect_block) return;

        uint32_t* list = (uint32_t*)malloc(this->block_size);
        uint64_t total_entries = this->block_size / sizeof(uint32_t);

        for (uint32_t i = 0; i < total_entries; i++){
            if (list[i] == 0) continue; // Either the end or a hole
            
            _free_double_indirect(list[i]);
        }

        _free_block(triple_indirect_block);
    }

    void ext2_t::_free_double_indirect(uint32_t double_indirect_block){
        if (!double_indirect_block) return;

        uint32_t* list = (uint32_t*)malloc(this->block_size);
        uint64_t total_entries = this->block_size / sizeof(uint32_t);

        for (uint32_t i = 0; i < total_entries; i++){
            if (list[i] == 0) continue; // Either the end or a hole
            
            _free_single_indirect(list[i]);
        }
        
        _free_block(double_indirect_block);
    }

    void ext2_t::_free_single_indirect(uint64_t single_indirect_block){
        if (!single_indirect_block) return;

        _free_block(single_indirect_block);
    }
    
    uint64_t ext2_t::get_inode_size(ext2_inode_t* inode){
        uint64_t size = inode->size_low;

        if (this->extended_superblock->ro_features & 0x0002) size |= (uint64_t)inode->size_upper << 32;

        return size;
    }


    int64_t ext2_t::load_inode_contents(uint32_t inode_num, uint64_t offset, uint64_t size, void* buffer) {
        ext2_inode_t* inode = fetch_inode(inode_num);
        
        // Safety check: Don't read past the file size
        uint64_t file_size = get_inode_size(inode);
        if (offset >= file_size) {
            delete inode;
            return 0;
        }

        uint32_t logical_block = offset / this->block_size;
        uint32_t offset_in_block = offset % this->block_size; // Offset into the very first block

        uint32_t total_read = 0;
        uint32_t total_to_read = min(size, file_size - offset);

        while (total_read < total_to_read) {
            // Get the starting physical block
            uint32_t start_phys_block = _get_physical_block(inode, logical_block);

            // Handle Sparse Files: Block 0 means "all zeros" in Ext2
            if (start_phys_block == 0) {
                // Determine how much to zero out for this block
                uint32_t remaining_in_block = this->block_size - offset_in_block;
                uint32_t zero_len = min((uint64_t)remaining_in_block, (uint64_t)(total_to_read - total_read));
                
                memset((char*)buffer + total_read, 0, zero_len);
                
                total_read += zero_len;
                logical_block++;
                offset_in_block = 0; // Subsequent blocks start at 0
                continue;
            }

            uint32_t contiguous_count = 1;
            
            uint64_t bytes_needed = total_to_read - total_read;
            
            while (true) {
                // Calculate bytes covered by current 'contiguous_count'
                uint64_t bytes_covered = ((uint64_t)contiguous_count * this->block_size) - offset_in_block;
                
                // If we already cover the requested size, stop looking
                if (bytes_covered >= bytes_needed) break;

                // Check the next block
                uint32_t next_logical = logical_block + contiguous_count;
                uint32_t next_phys = _get_physical_block(inode, next_logical);

                // Check whether its physically contiguous
                if (next_phys == start_phys_block + contiguous_count) {
                    contiguous_count++;
                } else {
                    break; // Not contiguous, stop here
                }
            }

            // Calculate Read Size
            uint64_t disk_offset = _block_to_offset(start_phys_block) + offset_in_block;
            uint64_t run_length_bytes = ((uint64_t)contiguous_count * this->block_size) - offset_in_block;
            uint64_t actual_read_size = min(bytes_needed, run_length_bytes);

            // Perform the single bulk read
            if (!blk->read(disk_offset, actual_read_size, (char*)buffer + total_read)) {
                delete inode; 
                return -EIO;
            }

            // Advance counters
            total_read += actual_read_size;
            logical_block += contiguous_count;
            offset_in_block = 0; // After the first read, we are always aligned to block boundaries
        }

        delete inode;
        return total_read;
    }

    int64_t ext2_t::write_inode_contents(uint32_t inode_num, uint64_t offset, uint64_t size, const void* buffer) {
        if (size == 0) return 0;

        ext2_inode_t* inode = fetch_inode(inode_num);
        if (!inode) return -ENOENT;

        // Calculate locality hint: Try to allocate blocks in the same group as the inode
        uint32_t inode_group = (inode_num - 1) / this->superblock->inodes_per_block_group;

        uint32_t logical_block = offset / this->block_size;
        uint32_t offset_in_block = offset % this->block_size;

        uint64_t total_written = 0;
        bool inode_modified = false;

        while (total_written < size) {
            // Resolve Logical -> Physical
            uint32_t physical_block = _get_physical_block(inode, logical_block);

            // Handle Missing Blocks (Holes or EOF)
            if (physical_block == 0) {
                int64_t alloc_ret = _allocate_block(inode_group);
                if (alloc_ret < 0) {
                    // Disk full or error. Stop here and save what we have done so far.
                    break; 
                }
                physical_block = (uint32_t)alloc_ret;

                // Link the new block to the inode
                // (Assuming _set_logical_block handles Direct/Indirect/Doubly-Indirect logic)
                if (!_set_physical_block(inode, logical_block, physical_block)) {
                    _free_block(physical_block); // Rollback
                    break;
                }

                // Update sector count (Standard Ext2 uses 512-byte sectors for this field)
                inode->sectors_used += (this->block_size / 512);
                inode_modified = true;
            }

            // Calculate Write Size for this iteration
            uint32_t remaining_on_block = this->block_size - offset_in_block;
            uint64_t to_write = min(size - total_written, (uint64_t)remaining_on_block);
            
            uint64_t phys_offset = _block_to_offset(physical_block) + offset_in_block;

            // Perform the Write
            if (!blk->write(phys_offset, to_write, (const char*)buffer + total_written)) {
                // IO Error during write
                break;
            }

            total_written += to_write;
            logical_block++;
            offset_in_block = 0; // Reset for subsequent blocks
        }

        // Update File Size
        // If we wrote past the current EOF, update the size
        uint64_t current_size = get_inode_size(inode);
        uint64_t end_pos = offset + total_written;

        if (end_pos > current_size) {
            inode->last_modification_time = current_time;
            inode->size_low = (uint32_t)end_pos;
            if (this->extended_superblock->ro_features & 0x0002) inode->size_upper = end_pos << 32;
            inode_modified = true;
        }

        // Commit Inode Changes
        // Only write the inode back if we changed size or allocated blocks
        if (inode_modified) {
            update_inode(inode_num, inode); // Assuming you have a _save/_write helper
        }

        delete inode;

        // Return error if we couldn't write anything at all
        if (total_written == 0 && size > 0) return -ENOSPC;

        return total_written;
    }

    int ext2_t::truncate_inode(uint32_t inode_num, uint64_t new_size) {
        ext2_inode_t* inode = fetch_inode(inode_num);
        if (!inode) return -ENOENT;

        uint64_t old_size = get_inode_size(inode);
        if (new_size == old_size) {
            delete inode;
            return 0;
        }

        // Zero the tail
        uint64_t boundary_limit = (new_size < old_size) ? new_size : old_size;
        
        if (boundary_limit % this->block_size != 0) {
            uint32_t last_logical_blk = boundary_limit / this->block_size;
            uint32_t offset_in_blk    = boundary_limit % this->block_size;
            
            uint32_t phys = _get_physical_block(inode, last_logical_blk);
            if (phys != 0) {
                uint8_t* buf = (uint8_t*)malloc(this->block_size);
                blk->read(_block_to_offset(phys), this->block_size, buf);
                
                // Zero everything after the valid data
                memset(buf + offset_in_blk, 0, this->block_size - offset_in_blk);
                
                blk->write(_block_to_offset(phys), this->block_size, buf);
                free(buf);
            }
        }

        // Calculate Block Counts
        uint32_t old_block_count = (old_size + this->block_size - 1) / this->block_size;
        uint32_t new_block_count = (new_size + this->block_size - 1) / this->block_size;

        if (new_size < old_size) {
            for (uint32_t i = old_block_count; i > new_block_count; i--) {
                uint32_t logical_idx = i - 1; 
                uint32_t phys_block = _get_physical_block(inode, logical_idx);
                
                if (phys_block != 0) {
                    _free_block(phys_block);
                    
                    // Update Metadata
                    uint32_t sectors_per_blk = this->block_size / 512;
                    if (inode->sectors_used >= sectors_per_blk) 
                        inode->sectors_used -= sectors_per_blk;
                    else 
                        inode->sectors_used = 0;
                    
                    _set_physical_block(inode, logical_idx, 0);
                }
            }
        } 
        else if (new_size > old_size) {
            // Use the inode's own group as a locality hint
            uint32_t group_hint = (inode_num - 1) / this->superblock->inodes_per_block_group;

            // Buffer for zeroing new blocks
            uint8_t* zero_buf = (uint8_t*)malloc(this->block_size);
            memset(zero_buf, 0, this->block_size);

            for (uint32_t logical_idx = old_block_count; logical_idx < new_block_count; logical_idx++) {
                
                int64_t phys_block = _allocate_block(group_hint);
                if (phys_block <= 0) {
                    // Out of space!
                    new_size = logical_idx * this->block_size; 
                    free(zero_buf);
                    break;
                }

                if (!blk->write(_block_to_offset(phys_block), this->block_size, zero_buf)) {
                    _free_block(phys_block);
                    free(zero_buf);
                    delete inode;
                    return -EIO;
                }

                // Link to Inode
                if (!_set_physical_block(inode, logical_idx, (uint32_t)phys_block)) {
                    _free_block(phys_block);
                    free(zero_buf);
                    delete inode;
                    return -EIO;
                }

                // Update Metadata
                inode->sectors_used += (this->block_size / 512);
            }
            free(zero_buf);
        }

        // Commit Changes
        inode->size_low = (uint32_t)new_size;
        if (this->extended_superblock->ro_features & 0x0002) 
            inode->size_upper = (uint32_t)(new_size >> 32);

        update_inode(inode_num, inode);
        delete inode;
        
        return 0;
    }

    // Frees the inode & its contents
    int ext2_t::free_inode_contents(uint32_t inode){
        ext2_inode_t* node = fetch_inode(inode);
        if (!node) return -ENOENT;

        if ((node->type_permissions & 0xF000) == EXT2_INODE_SYM && get_inode_size(node) <= 60) {
            node->deletion_time = current_time;
            update_inode(inode, node);
            _free_inode(inode);
            delete node;
            return 0;
        }

        uint32_t total_blocks = DIV_ROUND_UP(get_inode_size(node), this->block_size);

        for (uint32_t b = 0; b < total_blocks; b++){
            uint32_t phys = _get_physical_block(node, b);
            if (phys){
                _free_block(phys);
            }
        }

        if (node->single_indirect_block_pointer) _free_single_indirect(node->single_indirect_block_pointer);
        if (node->double_indirect_block_pointer) _free_double_indirect(node->double_indirect_block_pointer);
        if (node->triple_indirect_block_pointer) _free_triple_indirect(node->triple_indirect_block_pointer);

        node->deletion_time = current_time;
        update_inode(inode, node);
        _free_inode(inode);
        delete node;
        return 0;
    }
}