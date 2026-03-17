#include <filesystem/ext2/ext2.h>
#include <memory/heap.h>
#include <kstdio.h>
#include <drivers/timers/common.h>
#include <cstr.h>
#include <math.h>
#include <kerrno.h>


namespace filesystems{
    uint64_t ext2_t::_block_to_offset(uint32_t block){
        return (uint64_t)block * this->block_size;
    }

    int64_t ext2_t::_allocate_block(uint32_t start_group) {
        uint64_t groups_count = this->block_group_count;
        void* bitmap = malloc(this->block_size);
        
        uint32_t original_start_group = start_group;
        uint32_t current_start = start_group;
        uint32_t current_end = groups_count;

        // Lock the whole operation to prevent race conditions on the bitmap
        uint64_t rflags = spin_lock(&this->block_group_table_lock);

        for (int i = 0; i < 2; i++) {
            for (uint32_t indx = current_start; indx < current_end; indx++) {
                ext2_block_group_descriptor_t* group = &block_group_table[indx];
                
                if (group->unallocated_block_count == 0) continue;

                // Read Bitmap
                if (blk->read(_block_to_offset(group->block_of_block_usage_bitmap), this->block_size, bitmap) < 0) {
                    free(bitmap);
                    spin_unlock(&this->block_group_table_lock, rflags);
                    return -EIO;
                }

                uint64_t blocks_per_group = this->superblock->blocks_per_block_group;

                for (uint64_t b = 0; b < blocks_per_group; b++) {
                    uint32_t* bitmask = (uint32_t*)bitmap + (b / 32);
                    
                    if ((*bitmask & (1 << (b % 32))) == 0) {
                        // --- FOUND FREE BLOCK --- //

                        // Mark Used
                        *bitmask |= (1 << (b % 32));

                        if (blk->write(_block_to_offset(group->block_of_block_usage_bitmap), this->block_size, bitmap) < 0) {
                            free(bitmap);
                            spin_unlock(&this->block_group_table_lock, rflags);
                            return -EIO;
                        }

                        // Update In-Memory Counts
                        group->unallocated_block_count--;
                        this->superblock->unallocated_block_count--;

                        _save_block_group_table(); 
                        _save_superblock();

                        // Cleanup & Unlock
                        free(bitmap);
                        spin_unlock(&this->block_group_table_lock, rflags);

                        return this->superblock->first_data_block + b + (indx * blocks_per_group);
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

    void ext2_t::_free_block(uint32_t block) {
        // Never let the driver free the Superblock (Block 1 or 0) or the Boot Sector (Block 0).
        if (block <= this->superblock->first_data_block) return;

        uint64_t rflags = spin_lock(&this->block_group_table_lock);

        uint32_t real_index = block - this->superblock->first_data_block;
        uint32_t block_group = real_index / this->superblock->blocks_per_block_group;
        uint32_t rel_block   = real_index % this->superblock->blocks_per_block_group;

        if (block_group >= this->block_group_count) {
            spin_unlock(&this->block_group_table_lock, rflags);
            return;
        }

        ext2_block_group_descriptor_t* group = &block_group_table[block_group];
        
        void* bitmap = malloc(this->block_size);
        if (!blk->read(_block_to_offset(group->block_of_block_usage_bitmap), this->block_size, bitmap)){
            free(bitmap);
            spin_unlock(&this->block_group_table_lock, rflags); // FIX 3: Unlock on error
            return;
        }

        uint32_t* bitmask = (uint32_t*)bitmap + (rel_block / 32);
        
        // Check if the bit is actually set (prevent double-free corruption)
        if (*bitmask & (1 << (rel_block % 32))){
            
            // Clear the bit (Free it)
            *bitmask &= ~(1 << (rel_block % 32));

            if (!blk->write(_block_to_offset(group->block_of_block_usage_bitmap), this->block_size, bitmap)){
                free(bitmap);
                spin_unlock(&this->block_group_table_lock, rflags); // FIX 3: Unlock on error
                return;
            }

            // Update In-Memory Counts
            group->unallocated_block_count++;
            this->superblock->unallocated_block_count++;

            _save_block_group_table(); 
            _save_superblock();
        }

        free(bitmap);
        spin_unlock(&this->block_group_table_lock, rflags);
    }

    // Returns: Starting Physical Block ID (or negative error)
    // Arguments:
    //   - start_group: Where to start searching
    //   - requested_count: How many blocks we WANT
    //   - out_allocated_count: How many blocks we actually allocated (Output Pointer)
    int64_t ext2_t::_allocate_blocks(uint32_t start_group, uint32_t requested_count, uint32_t* out_allocated_count) {
        if (requested_count == 0) return -EINVAL;
        if (requested_count > this->superblock->unallocated_block_count) return -ENOSPC;

        // Sanity: We can't allocate more than fits in a single group contiguously
        if (requested_count > this->superblock->blocks_per_block_group) 
            requested_count = this->superblock->blocks_per_block_group;

        uint64_t groups_count = this->block_group_count;
        void* bitmap = malloc(this->block_size);
        
        // State to track the "Best Run" found so far
        uint32_t best_run_len = 0;
        uint32_t best_run_group = 0;
        uint32_t best_run_start_bit = 0;

        uint32_t current_start = start_group;
        uint32_t current_end = groups_count;
        uint32_t original_start = start_group;

        uint64_t rflags = spin_lock(&this->block_group_table_lock);

        for (int i = 0; i < 2; i++) {
            for (uint32_t indx = current_start; indx < current_end; indx++) {
                ext2_block_group_descriptor_t* group = &block_group_table[indx];
                
                // Skip groups that are totally full
                if (group->unallocated_block_count == 0) continue;

                // If a group has fewer free blocks than our current best run, skip it
                if (group->unallocated_block_count < best_run_len) continue;

                if (blk->read(_block_to_offset(group->block_of_block_usage_bitmap), this->block_size, bitmap) < 0) {
                    free(bitmap); spin_unlock(&this->block_group_table_lock, rflags); return -EIO;
                }

                uint64_t blocks_per_group = this->superblock->blocks_per_block_group;
                
                uint32_t current_run = 0;
                uint32_t run_start = 0;
                bool inside_run = false;

                for (uint64_t b = 0; b < blocks_per_group; b++) {
                    uint32_t* bitmask = (uint32_t*)bitmap + (b / 32);
                    bool is_used = (*bitmask & (1 << (b % 32)));

                    if (!is_used) {
                        if (!inside_run) {
                            inside_run = true;
                            run_start = b;
                            current_run = 1;
                        } else {
                            current_run++;
                        }

                        if (current_run == requested_count) {
                            goto allocate_now;
                        }
                    } else {
                        if (inside_run) {
                            if (current_run > best_run_len) {
                                best_run_len = current_run;
                                best_run_group = indx;
                                best_run_start_bit = run_start;
                            }
                        }
                        inside_run = false;
                        current_run = 0;
                    }
                }
                if (inside_run && current_run > best_run_len) {
                    best_run_len = current_run;
                    best_run_group = indx;
                    best_run_start_bit = run_start;
                }

                if (false) {
                allocate_now:
                    best_run_len = requested_count;
                    best_run_group = indx;
                    best_run_start_bit = run_start;
                    goto perform_allocation;
                }
            }
            current_start = 0;
            current_end = original_start;
        }

        if (best_run_len == 0) {
            free(bitmap);
            spin_unlock(&this->block_group_table_lock, rflags);
            return -ENOSPC;
        }

    perform_allocation:
        ext2_block_group_descriptor_t* target_group = &block_group_table[best_run_group];
        
        if (blk->read(_block_to_offset(target_group->block_of_block_usage_bitmap), this->block_size, bitmap) < 0) {
            free(bitmap); spin_unlock(&this->block_group_table_lock, rflags); return -EIO;
        }

        // Mark bits as used
        for (uint32_t k = 0; k < best_run_len; k++) {
            uint32_t b = best_run_start_bit + k;
            uint32_t* bitmask = (uint32_t*)bitmap + (b / 32);
            *bitmask |= (1 << (b % 32));
        }

        // Write Back
        if (blk->write(_block_to_offset(target_group->block_of_block_usage_bitmap), this->block_size, bitmap) < 0) {
            free(bitmap); spin_unlock(&this->block_group_table_lock, rflags); return -EIO;
        }

        // Update Counts
        target_group->unallocated_block_count -= best_run_len;
        this->superblock->unallocated_block_count -= best_run_len;

        _save_block_group_table();
        _save_superblock();

        free(bitmap);
        spin_unlock(&this->block_group_table_lock, rflags);

        // Set output size
        if (out_allocated_count) *out_allocated_count = best_run_len;

        // Return Physical Start ID
        uint64_t blocks_per_group = this->superblock->blocks_per_block_group;
        return this->superblock->first_data_block + best_run_start_bit + (best_run_group * blocks_per_group);
    }

}