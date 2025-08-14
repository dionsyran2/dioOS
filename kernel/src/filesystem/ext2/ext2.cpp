#include <filesystem/ext2/ext2.h>
#include <paging/PageFrameAllocator.h>
#include <memory.h>
#include <BasicRenderer.h>
#include <drivers/rtc/rtc.h>
#include <scheduling/apic/apic.h>
#include <math.h>
#include <kerrno.h>
#include <memory.h>

namespace filesystem{

    int64_t ext2_def_read(void* buffer, size_t cnt, size_t offset, vnode_t* this_node){
        serialf("\e[0;33m [EXT2]\e[0m Reading %s\n", this_node->name);
        ext2* fs = (ext2*)this_node->misc_data[0];

        ext2_inode* inode = fs->read_inode(this_node->inode);

        uint32_t c = 0;
        int64_t ret = fs->load_inode(inode, buffer, cnt, offset);
        this_node->last_modified = inode->last_modification_time;
        this_node->last_accessed = inode->last_access_time;
        delete inode;
        
        return ret;
    }

    int ext2_mkdir(const char* fn, vnode* this_node){
        ext2* fs = (ext2*)this_node->misc_data[0];
        ext2_directory* directory = nullptr;
        int ret = fs->mkdir(this_node->inode, fn, &directory);
        
        if (ret == 0){
            ext2_inode* inode = fs->read_inode(directory->inode);
            vnode_t* newdir = fs->create_and_mount_dir(directory, inode, this_node);
            fs->mount_dir(directory->inode, newdir, true);
            delete inode;
        }
        return ret;
    }

    int ext2_creat(const char* fn, vnode* this_node){
        ext2* fs = (ext2*)this_node->misc_data[0];
        ext2_directory* directory = nullptr;
        int ret = fs->create_file(this_node->inode, fn, &directory);
        
        if (ret == 0){
            ext2_inode* inode = fs->read_inode(directory->inode);
            vnode_t* newdir = fs->create_and_mount_dir(directory, inode, this_node);
            delete inode;
        }
        return ret;
    }

    int ext2_save_changed(vnode* this_node){
        ext2* fs = (ext2*)this_node->misc_data[0];
        ext2_inode* inode = fs->read_inode(this_node->inode);
        this_node->last_modified = inode->last_modification_time;
        this_node->last_accessed = inode->last_access_time;
        this_node->size = ((uint64_t)inode->size_upper << 32) | inode->size_low;
        this_node->gid = this_node->gid;
        this_node->uid = this_node->uid;

        fs->save_inode_entry(this_node->inode, inode);
        delete[] inode;
    }

    int64_t ext2_write(const void* buffer, size_t cnt, size_t offset, vnode* this_node){
        ext2* fs = (ext2*)this_node->misc_data[0];
        int64_t ret = fs->write_inode(this_node->inode, buffer, cnt, offset);
        if (ret < 0) return ret;
        ext2_inode* inode = fs->read_inode(this_node->inode);
        this_node->last_modified = inode->last_modification_time;
        this_node->last_accessed = inode->last_access_time;
        this_node->size = ((uint64_t)inode->size_upper << 32) | inode->size_low;
        delete[] inode;
        return ret;
    }

    int64_t ext2_truncate(uint64_t size, vnode* this_node){
        ext2* fs = (ext2*)this_node->misc_data[0];
        ext2_inode* inode = fs->read_inode(this_node->inode);
        int64_t ret = this_node->size;
        if (size < this_node->size){
            ret = fs->inode_truncate(inode, DIV_ROUND_UP(size, fs->block_size));
        }else{
            void* buffer = malloc(size - this_node->size);
            memset(buffer, 0, size - this_node->size);
            ret = ext2_write(buffer, size - this_node->size, this_node->size, this_node);
            free(buffer);
            if (ret > 0) ret = size;
        }
        if (ret < 0) return ret;

        inode->last_access_time = inode->last_modification_time = to_unix_timestamp(c_time);

        this_node->last_modified = inode->last_modification_time;
        this_node->last_accessed = inode->last_access_time;
        this_node->size = size;
        fs->save_inode_entry(this_node->inode, inode);

        delete inode;
        return size;
    }

    int ext2_read_dir(vnode** out_list, vnode* this_node){
        ext2* fs = (ext2*)this_node->misc_data[0];
        if (this_node->type != VDIR) return -ENOTDIR;

        return fs->read_dir(this_node->inode, out_list); 
    }

    bool is_ext2(vblk_t* blk, uint64_t start_block){
        uint16_t superblock_offset_in_blocks = EXT2_SUPERBLOCK_OFFSET / blk->block_size; // (device blocks)
        uint16_t superblock_size_in_sectors = DIV_ROUND_UP(EXT2_SUPERBLOCK_SIZE, blk->block_size);

        ext2_superblock* sb = (ext2_superblock*)GlobalAllocator.RequestPages(((superblock_size_in_sectors * blk->block_size) / 0x1000) + 1);
        void* raw = (void*)sb;
        blk->read(start_block + superblock_offset_in_blocks, superblock_size_in_sectors, (void*)sb);
        if (superblock_offset_in_blocks == 0) sb = (ext2_superblock*)((uint64_t)sb + EXT2_SUPERBLOCK_OFFSET);

        bool ret = false;
        if (sb->signature == 0xef53) ret = true;

        GlobalAllocator.FreePages(raw, ((superblock_size_in_sectors * blk->block_size) / 0x1000) + 1);
        return ret;
    }

    uint64_t ext2::_block_to_sector(uint64_t block){
        return start_sector + (block * sectors_per_block);
    }

    ext2::ext2(vblk_t* blk, uint64_t start_block, uint64_t last_block, PartEntry* gpt_entry){
        this->blk = blk;
        this->start_sector = start_block;
        this->last_sector = last_block;

        if (gpt_entry){
            memcpy(partition_guid, gpt_entry->UNIQUE_PARTITION_GUID, sizeof(gpt_entry->UNIQUE_PARTITION_GUID));
        }

        _load_superblock();
        _parse_superblock();
        _load_block_group_descriptor();

        kprintf(0xF00F0F, "[ext2] ");
        kprintf("Volume version: %d.%d\n", superblock->major_version, superblock->minor_version);
        kprintf(0xFF6A00, "[ext2] ");
        kprintf("Mounted '%s' at '%s'\n", gpt_entry ? conv_wchar((wchar_t*)gpt_entry->PARTITION_NAME) : "NO NAME", _mount_fs());
    }

    void ext2::_parse_superblock(){
        inode_size_in_bytes = superblock->major_version >= 1 ? superblock->extended.size_of_inode : 128;
        inodes_per_group = superblock->num_of_inodes_in_group;
        block_size = 1024 << superblock->block_size;
        sectors_per_block = block_size / blk->block_size;
    }

    void ext2::_load_superblock(){
        uint16_t superblock_offset_in_sectors = EXT2_SUPERBLOCK_OFFSET / blk->block_size; // (device blocks)
        superblock_size_in_sectors = DIV_ROUND_UP(EXT2_SUPERBLOCK_SIZE, blk->block_size);
        superblock_sector = start_sector + superblock_offset_in_sectors;
        superblock_size_in_bytes = superblock_size_in_sectors * blk->block_size;
        superblock_size_in_pages = DIV_ROUND_UP(superblock_size_in_bytes, 0x1000);

        superblock = (ext2_superblock*)GlobalAllocator.RequestPages(superblock_size_in_pages);
        blk->read(superblock_sector, superblock_size_in_sectors, superblock);
        if (superblock_offset_in_sectors == 0) superblock = (ext2_superblock*)((uint64_t)superblock + EXT2_SUPERBLOCK_OFFSET);
        if (superblock->signature != 0xef53) kprintf("Invalid Signature\n");

    }

    void ext2::_save_superblock(){
        superblock->last_mount_time = to_unix_timestamp(c_time->second, c_time->minute, c_time->hour, c_time->day, c_time->month, c_time->year);
        superblock->num_of_times_mounted++;
        
        blk->write(superblock_sector, superblock_size_in_sectors, superblock);
    }

    void ext2::_load_block_group_descriptor() {
        block_group_count = DIV_ROUND_UP(superblock->num_of_blocks, superblock->num_of_blocks_in_group);

        uint32_t bgdt_block = (blk->block_size == 1024) ? 2 : 1;

        block_group_table_bytes = block_group_count * sizeof(ext2_block_group_descriptor);
        block_group_table_size_in_sectors = DIV_ROUND_UP(block_group_table_bytes, blk->block_size);
        block_group_table_size_in_pages = DIV_ROUND_UP(block_group_table_bytes, 0x1000);

        block_group_table = (ext2_block_group_descriptor*)GlobalAllocator.RequestPages(block_group_table_size_in_pages);

        blk->read(_block_to_sector(bgdt_block), block_group_table_size_in_sectors, block_group_table);
    }

    void ext2::_save_block_group_descriptor() {
        uint32_t bgdt_block = (blk->block_size == 1024) ? 2 : 1;

        blk->write(_block_to_sector(bgdt_block), block_group_table_size_in_sectors, block_group_table);
    }

    uint32_t ext2::_number_of_blocks_indirect_l1(uint32_t singly_indirect_block_pointer) {
        if (singly_indirect_block_pointer == 0) return 0;

        uint32_t count = 0;

        uint32_t num_of_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));

        blk->read(_block_to_sector(singly_indirect_block_pointer), sectors_per_block, table);

        for (int i = 0; i < num_of_entries; i++){
            uint32_t ptr = table[i];
            if (ptr != 0 && ptr != (uint32_t)-1) count++;
        }

        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));

        return count;
    }

    uint32_t ext2::_number_of_blocks_indirect_l2(uint32_t doubly_indirect_block_pointer){
        if (doubly_indirect_block_pointer == 0) return 0;

        uint32_t count = 0;

        uint32_t num_of_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));

        blk->read(_block_to_sector(doubly_indirect_block_pointer), sectors_per_block, table);

        for (int i = 0; i < num_of_entries; i++){
            uint32_t ptr = table[i];
            if (ptr != 0 && ptr != (uint32_t)-1) count += _number_of_blocks_indirect_l1(ptr);
        }
        
        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));

        return count;
    }

    uint32_t ext2::_number_of_blocks_indirect_l3(uint32_t triple_indirect_block_pointer){
        if (triple_indirect_block_pointer == 0) return 0;

        uint32_t count = 0;

        uint32_t num_of_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));

        blk->read(_block_to_sector(triple_indirect_block_pointer), sectors_per_block, table);

        for (int i = 0; i < num_of_entries; i++){
            uint32_t ptr = table[i];
            if (ptr != 0 && ptr != (uint32_t)-1) count += _number_of_blocks_indirect_l2(ptr);
        }
        
        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));

        return count;
    }

    uint32_t ext2::_number_of_blocks_direct(ext2_inode* inode){
        if (inode == nullptr) return 0;
        uint32_t count = 0;

        for (int i = 0; i < 12; i++){
            uint32_t ptr = inode->direct_block_pointer[i];
            if (ptr != 0 && ptr != (uint32_t)-1) count++;
        }

        return count;
    }
    uint32_t ext2::_number_of_blocks(ext2_inode* inode){
        if (inode == nullptr) return 0;
        uint32_t count = 0;
        count += _number_of_blocks_direct(inode);
        count += _number_of_blocks_indirect_l1(inode->singly_indirect_block_pointer);
        count += _number_of_blocks_indirect_l2(inode->doubly_indirect_block_pointer);
        count += _number_of_blocks_indirect_l3(inode->triply_indirect_block_pointer);
        return count;
    }

    void ext2::_push_block_table_direct(ext2_inode* inode, uint32_t* table, uint32_t* offset){
        if (inode == nullptr) return;
        uint16_t count = 0;

        for (int i = 0; i < 12; i++){
            uint32_t ptr = inode->direct_block_pointer[i];
            if (ptr == 0 || ptr == (uint32_t)-1) continue;
            table[*offset] = ptr;
            (*offset)++;        
        }
    }

    void ext2::_push_block_table_indirect_l1(uint32_t singly_indirect_block_pointer, uint32_t* out, uint32_t* offset){
        if (singly_indirect_block_pointer == 0) return;

        uint32_t num_of_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));

        blk->read(_block_to_sector(singly_indirect_block_pointer), sectors_per_block, table);

        for (int i = 0; i < num_of_entries; i++){
            uint32_t ptr = table[i];
            if (ptr == 0 || ptr == (uint32_t)-1) continue;
            out[*offset] = ptr;
            (*offset)++;
        }

        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));
    }

    void ext2::_push_block_table_indirect_l2(uint32_t doubly_indirect_block_pointer, uint32_t* out, uint32_t* offset){
        if (doubly_indirect_block_pointer == 0) return;

        uint32_t num_of_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));

        blk->read(_block_to_sector(doubly_indirect_block_pointer), sectors_per_block, table);

        for (int i = 0; i < num_of_entries; i++){
            uint32_t ptr = table[i];
            if (ptr != 0 && ptr != (uint32_t)-1) _push_block_table_indirect_l1(ptr, out, offset);
        }
        
        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));
    }
    void ext2::_push_block_table_indirect_l3(uint32_t triply_indirect_block_pointer, uint32_t* out, uint32_t* offset){
        if (triply_indirect_block_pointer == 0) return;


        uint32_t num_of_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));

        blk->read(_block_to_sector(triply_indirect_block_pointer), sectors_per_block, table);

        for (int i = 0; i < num_of_entries; i++){
            uint32_t ptr = table[i];
            if (ptr != 0 && ptr != (uint32_t)-1) _push_block_table_indirect_l2(ptr, out, offset);
        }
        
        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));
    }

    uint32_t* ext2::_get_block_table(ext2_inode* inode, uint32_t* length){
        if (inode == nullptr) return 0;
        uint32_t* table = new uint32_t[_number_of_blocks(inode)];
        uint32_t offset = 0;
        _push_block_table_direct(inode, table, &offset);
        _push_block_table_indirect_l1(inode->singly_indirect_block_pointer, table, &offset);
        _push_block_table_indirect_l2(inode->doubly_indirect_block_pointer, table, &offset);
        _push_block_table_indirect_l3(inode->triply_indirect_block_pointer, table, &offset);
        *length = offset;
        return table;
    }

    ext2_inode* ext2::read_inode(uint32_t inode){
        uint32_t block_group_index = (inode - 1) / superblock->num_of_inodes_in_group;
        uint32_t inode_index = (inode - 1) % superblock->num_of_inodes_in_group;
        uint32_t containing_block = (inode_index * inode_size_in_bytes) / block_size;
        size_t offset = (inode_index * inode_size_in_bytes) % block_size;

        uint32_t inode_table_address = block_group_table[block_group_index].inode_table_block;
        uint32_t sectors_to_load = DIV_ROUND_UP(block_size, blk->block_size);

        uint32_t pages = DIV_ROUND_UP(sectors_to_load * blk->block_size, 0x1000);
        void* inode_table = GlobalAllocator.RequestPages(pages);
        memset(inode_table, 0, pages * 0x1000);

        blk->read(_block_to_sector(inode_table_address + containing_block), sectors_to_load, inode_table);

        ext2_inode* i_node = (ext2_inode*)((uint8_t*)inode_table + offset);
        ext2_inode* copy = (ext2_inode*)malloc(inode_size_in_bytes);
        memcpy(copy, i_node, inode_size_in_bytes);

        GlobalAllocator.FreePages(inode_table, pages);
        
        return copy;
    }

    bool ext2::save_inode_entry(uint32_t inode, ext2_inode* inode_entry){
        uint32_t block_group_index = (inode - 1) / superblock->num_of_inodes_in_group;
        uint32_t inode_index = (inode - 1) % superblock->num_of_inodes_in_group;
        uint32_t containing_block = (inode_index * inode_size_in_bytes) / block_size;
        size_t offset = (inode_index * inode_size_in_bytes) % block_size;

        uint32_t inode_table_address = block_group_table[block_group_index].inode_table_block;
        uint32_t sectors_to_load = DIV_ROUND_UP(block_size, blk->block_size);

        uint32_t pages = DIV_ROUND_UP(sectors_to_load * blk->block_size, 0x1000);
        void* inode_table = GlobalAllocator.RequestPages(pages);

        if (!blk->read(_block_to_sector(inode_table_address + containing_block), sectors_to_load, inode_table)) return false;

        ext2_inode* i_node = (ext2_inode*)((uint8_t*)inode_table + offset);
        memcpy(i_node, inode_entry, inode_size_in_bytes);

        if (!blk->write(_block_to_sector(inode_table_address + containing_block), sectors_to_load, inode_table)) return false;

        GlobalAllocator.FreePages(inode_table, pages);

        return true;
    }

    int64_t ext2::load_inode(ext2_inode* inode, void* buffer, size_t cnt, size_t offset){
        uint64_t total_size_of_inode = ((uint64_t)inode->size_upper << 32) | inode->size_low;

        uint32_t length = 0;
        uint32_t* blocks = _get_block_table(inode, &length);
        
        uint32_t block_offset = offset / block_size;
        uint32_t start_offset_in_bytes = offset % block_size;
        uint32_t length_in_blocks = DIV_ROUND_UP(cnt + start_offset_in_bytes, block_size);


        if (offset > total_size_of_inode) return EOF;
        if ((cnt + offset) > total_size_of_inode) cnt = (total_size_of_inode - offset);
        
        uint64_t offset_in_buffer = 0;
        uint64_t end_block = block_offset + length_in_blocks;
        if (end_block > length) end_block = length;
        for (uint32_t block = block_offset; block < end_block;){
            if (offset_in_buffer >= cnt || (offset_in_buffer + offset) >= total_size_of_inode) break;

            uint32_t blk_ptr = blocks[block];
            uint32_t num_of_blks = 1;
            uint32_t prev = blk_ptr;

            for (uint32_t b = block + 1; b < end_block; b++){
                if (blocks[b] != prev + 1) break;
                prev = blocks[b];
                num_of_blks++;
            }

            uint32_t bytes = num_of_blks * block_size;
            uint32_t sectors = num_of_blks * sectors_per_block;
            uint32_t sector = _block_to_sector(blk_ptr);


            void* tmp = GlobalAllocator.RequestPages(DIV_ROUND_UP(sectors * blk->block_size, 0x1000));
            memset(tmp, 0, sectors * blk->block_size);

            if (!blk->read(sector, sectors, tmp)) return -EFAULT;

            uint64_t off = ((block == block_offset) ? start_offset_in_bytes : 0);
            uint64_t to_copy = min(bytes - off, cnt - offset_in_buffer);
            to_copy = min(to_copy, total_size_of_inode - (offset_in_buffer + offset));
            //serialf("COPYING: %d %d %d\n", offset_in_buffer, to_copy, total_size_of_inode);
            memcpy((uint8_t*)buffer + offset_in_buffer, (uint8_t*)tmp + off, to_copy);
            GlobalAllocator.FreePages(tmp, DIV_ROUND_UP(sectors * blk->block_size, 0x1000));
            offset_in_buffer += to_copy;
            block += num_of_blks;
        }

        delete[] blocks;
        return offset_in_buffer;
    }

    int64_t ext2::write_inode(uint32_t inode_num, const void* buffer, size_t cnt, size_t offset){
        ext2_inode* inode = read_inode(inode_num);

        uint32_t length = 0;
        uint32_t* blocks = _get_block_table(inode, &length);
        uint64_t total_file_size = (offset + cnt);
        uint64_t required_blocks = DIV_ROUND_UP(total_file_size, block_size);
        if (length < required_blocks){
            uint32_t blk_array[required_blocks - length] = { 0 };
            uint32_t indx = 0;
            while (length < required_blocks){
                uint32_t blocks_found = 0;

                uint32_t block = _find_free_blocks(required_blocks - length, blocks_found);
                if (block == 0 || blocks_found == 0) return -EXFULL;
                
                _mark_blocks(block, blocks_found, true);
                for (int i = 0; i < blocks_found; i++){
                    //_inode_add_block(inode, block + i);
                    blk_array[indx++] = block + i;
                }
                
                inode->count_of_disk_sectors += sectors_per_block * blocks_found;
                length += blocks_found;
            }
            /*for (int i = 0; i < indx; i++){
                serialf("\e[31mBlock found:\e[0m %d\n", blk_array[i]);
            }*/
            //serialf("\e[34mAdding %d blocks total %d\e[0m\n", indx, length);
            _inode_add_blocks(inode, blk_array, indx);
            save_inode_entry(inode_num, inode);
            delete[] blocks;
            blocks = _get_block_table(inode, &length);
            /*for (int i = 0; i < length; i++){
                serialf("\e[33mBlock returned:\e[0m %d\n", blocks[i]);
            }*/
        }

        uint32_t block_offset = offset / block_size;
        uint32_t start_offset_in_bytes = offset % block_size;
        uint32_t length_in_blocks = DIV_ROUND_UP(cnt, block_size);

        uint64_t offset_in_buffer = 0;
        uint64_t end_block = min(block_offset + length_in_blocks, length);

        for (uint32_t block = block_offset; block < end_block;){
            if (offset_in_buffer >= cnt) break;
            uint32_t blk_ptr = blocks[block];
            uint32_t num_of_blks = 1;
            uint32_t prev = blk_ptr;

            for (uint32_t b = block + 1; b < end_block; b++){
                if (blocks[b] != prev + 1) break;
                prev = blocks[b];
                num_of_blks++;
            }

            uint32_t bytes = num_of_blks * block_size;
            uint32_t sectors = DIV_ROUND_UP(bytes, blk->block_size);
            uint32_t sector = _block_to_sector(blk_ptr);
            uint64_t off = block == block_offset ? start_offset_in_bytes : 0;
            uint64_t to_copy = min(bytes - off, cnt - offset_in_buffer);
            //serialf("\e[31mBlock: \e[0m%d\e[31m Sector: \e[0m%d\e[31m block_count:\e[0m %d\n", blk_ptr, sector, num_of_blks);
            void* tmp = GlobalAllocator.RequestPages(DIV_ROUND_UP(sectors * blk->block_size, 0x1000));
            if (off > 0 || to_copy < (bytes - off)){
                if(!blk->read(sector, sectors, tmp)) return -EFAULT;
            }
            
            memcpy_simd((uint8_t*)tmp + off, (uint8_t*)buffer + offset_in_buffer, to_copy);

            if (!blk->write(sector, sectors, tmp)) return -EFAULT;

            GlobalAllocator.FreePages(tmp, DIV_ROUND_UP(sectors * blk->block_size, 0x1000));
            offset_in_buffer += to_copy;
            block += num_of_blks;
        }

        delete[] blocks;

        // metadata
        inode->last_access_time = inode->last_modification_time = to_unix_timestamp(c_time);
        inode->size_low = (uint32_t)(total_file_size & 0xFFFFFFFF);
        inode->size_upper = (uint32_t)((total_file_size >> 32) & 0xFFFFFFFF);
        save_inode_entry(inode_num, inode);

        delete inode;
        
        return offset_in_buffer;
    }

    VNODE_TYPE ext2_type_to_vfs(uint32_t type){
        if (type & EXT2_FIFO) return VFIFO;
        if (type & EXT2_CHR) return VCHR;
        if (type & EXT2_DIR) return VDIR;
        if (type & EXT2_BLK) return VBLK;
        if (type & EXT2_REG) return VREG;
        if (type & EXT2_SYM) return VLNK;
        if (type & EXT2_SOC) return VSOC;
        return VBAD;
    }

    vnode_t* ext2::create_and_mount_dir(ext2_directory* dir, ext2_inode* inode, vnode_t* parent){
        char* name = new char[dir->name_length + 1];
        memcpy(name, dir->name, dir->name_length);
        name[dir->name_length] = '\0';
        vnode_t* node = vfs::mount_node(name, ext2_type_to_vfs(inode->type_perms), parent);        

        
        node->ops.read = ext2_def_read;
        node->ops.mkdir = ext2_mkdir;
        node->ops.creat = ext2_creat;
        node->ops.write = ext2_write;
        node->ops.truncate = ext2_truncate;
        node->ops.save_entry = ext2_save_changed;
        

        node->misc_data[0] = (void*)this;

        node->inode = dir->inode;
        node->gid = inode->group_id;
        node->uid = inode->user_id;
        node->perms = inode->type_perms & 0xFFF;
        node->creation_time = inode->creation_time;
        node->last_accessed = inode->last_access_time;
        node->last_modified = inode->last_modification_time;
        node->size = ((uint64_t)inode->size_upper << 32) | inode->size_low;
        
        delete name;
        return node;
    }

    void ext2::mount_dir(uint32_t inode, vnode_t* parent, bool sub){
        ext2_inode* ind = read_inode(inode);

        uint64_t fsz = ind->size_low | ((uint64_t)ind->size_upper << 32);
        void* data = GlobalAllocator.RequestPages(DIV_ROUND_UP(fsz, 0x1000));
        uint32_t data_read = load_inode(ind, data, fsz, 0);

        size_t offset = 0;
        while (offset < fsz){
            ext2_directory* entry = (ext2_directory*)((uint8_t*)data + offset);
            char name[entry->name_length + 1] = { 0 };
            memcpy(&name, entry->name, entry->name_length);

            if (entry->inode == 0 || entry->rec_len == 0) break;


            ext2_inode* ind = read_inode(entry->inode);
            
            vnode_t* node = create_and_mount_dir(entry, ind, parent);
            if (ind->type_perms & EXT2_DIR && sub){
                if (strcmp(".", name) != 0 && strcmp("..", name) != 0) mount_dir(entry->inode, node, sub);
            }
            
            delete ind;
            offset += entry->rec_len;
        }

        delete ind;
        GlobalAllocator.FreePages(data, DIV_ROUND_UP(fsz, 0x1000));
    }

    int ext2::read_dir(uint32_t inode, vnode_t** inode_out){
        if (get_dir_cache(inode) != nullptr){
            vnode_t* cached = get_cached_children(inode);
            *inode_out = cached;

            int cnt = 0;
            vnode_t* t = cached;
            while(t != nullptr){
                t = t->next;
                cnt++;
            }
            return cnt;
        }else{
            ext2_inode* ind = read_inode(inode);

            uint64_t fsz = ind->size_low | ((uint64_t)ind->size_upper << 32);
            void* data = GlobalAllocator.RequestPages(DIV_ROUND_UP(fsz, 0x1000));
            uint32_t data_read = load_inode(ind, data, fsz, 0);

            size_t offset = 0;
            vnode_t* list = nullptr;
            vnode_t* last = nullptr;
            int cnt = 0;
            while (offset < fsz){
                ext2_directory* entry = (ext2_directory*)((uint8_t*)data + offset);
                char name[entry->name_length + 1] = { 0 };
                memcpy(&name, entry->name, entry->name_length);

                if (entry->inode == 0 || entry->rec_len == 0) break;


                ext2_inode* d_inode = read_inode(entry->inode);
                
                
                vnode_t* node = new vnode_t;        
                memset(node, 0, sizeof(vnode_t));
                
                node->type = ext2_type_to_vfs(d_inode->type_perms);
                node->ops.read = ext2_def_read;
                node->ops.mkdir = ext2_mkdir;
                node->ops.creat = ext2_creat;
                node->ops.write = ext2_write;
                node->ops.truncate = ext2_truncate;
                node->ops.read_dir = ext2_read_dir;
                node->ops.save_entry = ext2_save_changed;

                node->misc_data[0] = (void*)this;

                node->inode = entry->inode;
                node->gid = d_inode->group_id;
                node->uid = d_inode->user_id;
                node->perms = d_inode->type_perms & 0xFFF;
                node->creation_time = d_inode->creation_time;
                node->last_accessed = d_inode->last_access_time;
                node->last_modified = d_inode->last_modification_time;
                node->size = ((uint64_t)d_inode->size_upper << 32) | d_inode->size_low;
                node->is_static = false;
                                
                strcpy(node->name, name);

                if (list == nullptr){
                    list = node;
                }else{
                    last->next = node;
                }

                last = node;
                cnt++;

                cache_vnode(inode, node);

                delete d_inode;

                offset += entry->rec_len;
            }

            delete ind;
            GlobalAllocator.FreePages(data, DIV_ROUND_UP(fsz, 0x1000));
            *inode_out = list;
            return cnt;
        }
    }

    char* ext2::_mount_fs(){
        vnode_t* fs = vfs::get_root_node();
        if (!vfs::is_root()){ // check if this should be mounted as the root partition, if not change the fs
            vnode_t* mnt = vfs::resolve_path("/mnt");
            if (mnt == nullptr){
                mnt = vfs::mount_node("mnt", VDIR, nullptr);
            }
            
            char* name = new char[strlen(blk->node.name) + 2];
            strcpy(name, blk->node.name);
            strcat(name, toString((uint8_t)(blk->num_of_partitions + 1)));
            blk->num_of_partitions++;
            fs = vfs::mount_node(name, VDIR, mnt);
        }
        fs->misc_data[0] = (void*)this;
        fs->inode = 2;
        fs->ops.mkdir = ext2_mkdir;
        fs->ops.creat = ext2_creat;
        fs->ops.read_dir = ext2_read_dir;
        //mount_dir(2, fs, true);
        return vfs::get_full_path_name(fs);
    }

    uint32_t ext2::_find_free_inode(){
        for (size_t i = 0; i < block_group_count; i++){
            ext2_block_group_descriptor* group = &block_group_table[i];

            if (group->unallocated_inode_count == 0) continue;

            int size_in_pages = DIV_ROUND_UP(sectors_per_block * blk->block_size, 0x1000);

            uint8_t* bitmap = (uint8_t*)GlobalAllocator.RequestPages(size_in_pages);
            blk->read(_block_to_sector(group->inode_usage_bitmap), sectors_per_block, bitmap);

            for (int b = (i == 0 ? superblock->extended.first_non_reserved_inode : 0); b < inodes_per_group; b++) {

                uint8_t byte = bitmap[b / 8];
                uint8_t mask = 1 << (b % 8);

                if ((byte & mask) == 0){
                    GlobalAllocator.FreePages(bitmap, size_in_pages);
                    //serialf("\e[0;32mFree inode: \e[0m%d\n", (i * inodes_per_group) + b + 1);
                    return (i * inodes_per_group) + b + 1;
                }
            }

            GlobalAllocator.FreePages(bitmap, size_in_pages);
        }
        return 0;
    }


    bool ext2::_mark_inode(uint32_t inode, bool v){
        uint32_t block_group_index = (inode - 1) / superblock->num_of_inodes_in_group;
        uint32_t inode_index = (inode - 1) % superblock->num_of_inodes_in_group;
        
        ext2_block_group_descriptor* group = &block_group_table[block_group_index];

        int size_in_pages = DIV_ROUND_UP(sectors_per_block * blk->block_size, 0x1000);

        uint8_t* bitmap = (uint8_t*)GlobalAllocator.RequestPages(size_in_pages);
        blk->read(_block_to_sector(group->inode_usage_bitmap), sectors_per_block, bitmap);

        uint8_t* byte = &bitmap[inode_index / 8];
        uint8_t mask = 1 << (inode_index % 8);
        
        bool ret = true;

        if (v){
            if ((*byte) & mask){
                ret = false;
                goto exit; 
            }

            *byte |= mask;
            group->unallocated_inode_count -= 1;
        }else{
            if (((*byte) & mask) == 0){
                ret = false;
                goto exit;
            }

            *byte &= ~mask;
            group->unallocated_inode_count += 1;
        }

        // write the changes to the disk
        blk->write(_block_to_sector(group->inode_usage_bitmap), sectors_per_block, bitmap);
        _save_block_group_descriptor();
        
    exit:
        GlobalAllocator.FreePages(bitmap, size_in_pages);
        return ret;
    }

    uint32_t ext2::_find_free_block(){
        uint32_t last_free_block_group_index = (last_allocated_block + 1) / superblock->num_of_blocks_in_group;
        uint32_t last_free_block_block_index = (last_allocated_block + 1) % superblock->num_of_blocks_in_group;

        for (size_t i = last_free_block_group_index; i < block_group_count; i++){
            ext2_block_group_descriptor* group = &block_group_table[i];

            if (group->unallocated_block_count == 0) continue;

            int size_in_pages = DIV_ROUND_UP(sectors_per_block * blk->block_size, 0x1000);

            uint8_t* bitmap = (uint8_t*)GlobalAllocator.RequestPages(size_in_pages);
            blk->read(_block_to_sector(group->block_usage_bitmap), sectors_per_block, bitmap);
            
            for (int b = last_free_block_block_index; b < superblock->num_of_blocks_in_group; b++){

                uint8_t byte = bitmap[b / 8];
                uint8_t mask = 1 << (b % 8);

                if ((byte & mask) == 0){
                    GlobalAllocator.FreePages(bitmap, size_in_pages);
                    if (_block_to_sector((i * superblock->num_of_blocks_in_group) + b) > blk->block_count) return 0;
                    //serialf("\e[0;32mFree block: \e[0m%d\n", (i * superblock->num_of_blocks_in_group) + b);
                    uint32_t block = (i * superblock->num_of_blocks_in_group) + b;
                    last_allocated_block = block;
                    return block;
                }
            }

            GlobalAllocator.FreePages(bitmap, size_in_pages);
        }
        return 0;
    }

    uint32_t ext2::_find_free_blocks(uint32_t count, uint32_t& actual_count_out) {
        uint32_t best_start_block = 0;
        uint32_t best_run_length = 0;

        uint32_t start_group = (last_allocated_block + 1) / superblock->num_of_blocks_in_group;
        uint32_t start_index = (last_allocated_block + 1) % superblock->num_of_blocks_in_group;

        for (size_t group_index = start_group; group_index < block_group_count; group_index++) {
            ext2_block_group_descriptor* group = &block_group_table[group_index];
            if (group->unallocated_block_count == 0) continue;

            int bitmap_pages = DIV_ROUND_UP(sectors_per_block * blk->block_size, 0x1000);
            uint8_t* bitmap = (uint8_t*)GlobalAllocator.RequestPages(bitmap_pages);
            blk->read(_block_to_sector(group->block_usage_bitmap), sectors_per_block, bitmap);

            uint32_t blocks_in_group = superblock->num_of_blocks_in_group;
            uint32_t group_block_start = group_index * blocks_in_group;
            uint32_t run_start = 0;
            uint32_t run_length = 0;

            for (uint32_t b = (group_index == start_group ? start_index : 0); b < blocks_in_group; b++) {
                uint8_t byte = bitmap[b / 8];
                uint8_t mask = 1 << (b % 8);
                bool is_allocated = byte & mask;

                if (!is_allocated) {
                    if (run_length == 0) run_start = b;
                    run_length++;

                    if (run_length == count) {
                        GlobalAllocator.FreePages(bitmap, bitmap_pages);
                        uint32_t found_block = group_block_start + run_start;
                        last_allocated_block = found_block + run_length - 1;
                        actual_count_out = run_length;
                        return found_block;
                    }
                } else {
                    if (run_length > best_run_length) {
                        best_run_length = run_length;
                        best_start_block = group_block_start + run_start;
                    }
                    run_length = 0;
                }
            }

            // Update best run in case it ends at group end
            if (run_length > best_run_length) {
                best_run_length = run_length;
                best_start_block = group_block_start + run_start;
            }

            GlobalAllocator.FreePages(bitmap, bitmap_pages);
        }

        // Fall back to best run found
        if (best_run_length > 0) {
            last_allocated_block = best_start_block + best_run_length - 1;
            actual_count_out = best_run_length;
            return best_start_block;
        }

        actual_count_out = 0;
        return 0;
    }


    bool ext2::_mark_block(uint32_t block, bool v){
        uint32_t block_group_index = block / superblock->num_of_blocks_in_group;
        if (block_group_index > block_group_count) {serialf("Number of block groups > block group index\n");return false;}
        uint32_t block_index = block % superblock->num_of_blocks_in_group;
        
        ext2_block_group_descriptor* group = &block_group_table[block_group_index];

        int size_in_pages = DIV_ROUND_UP(sectors_per_block * blk->block_size, 0x1000);

        uint8_t* bitmap = (uint8_t*)GlobalAllocator.RequestPages(size_in_pages);
        blk->read(_block_to_sector(group->block_usage_bitmap), sectors_per_block, bitmap);

        uint8_t* byte = &bitmap[block_index / 8];
        uint8_t mask = 1 << (block_index % 8);
        
        bool ret = true;

        if (v){
            if ((*byte) & mask){
                ret = false;
                goto exit; 
            }

            *byte |= mask;
            group->unallocated_block_count -= 1;
        }else{
            if (((*byte) & mask) == 0){
                ret = false;
                goto exit;
            }

            *byte &= ~mask;
            group->unallocated_block_count += 1;
        }

        // write the changes to the disk
        blk->write(_block_to_sector(group->block_usage_bitmap), sectors_per_block, bitmap);
        _save_block_group_descriptor();
        
    exit:
        GlobalAllocator.FreePages(bitmap, size_in_pages);
        return ret;
    }

    bool ext2::_mark_blocks(uint32_t start_block, uint32_t count, bool value) {
        if (count == 0) return true;

        while (count > 0) {
            uint32_t block_group_index = start_block / superblock->num_of_blocks_in_group;
            if (block_group_index >= block_group_count) {
                serialf("Block group index %d out of range (max %d)\n", block_group_index, block_group_count);
                return false;
            }

            uint32_t group_block_index = start_block % superblock->num_of_blocks_in_group;
            ext2_block_group_descriptor* group = &block_group_table[block_group_index];

            uint32_t blocks_in_group = superblock->num_of_blocks_in_group;
            uint32_t blocks_remaining_in_group = blocks_in_group - group_block_index;
            uint32_t to_process = (count < blocks_remaining_in_group) ? count : blocks_remaining_in_group;

            int size_in_pages = DIV_ROUND_UP(sectors_per_block * blk->block_size, 0x1000);
            uint8_t* bitmap = (uint8_t*)GlobalAllocator.RequestPages(size_in_pages);
            blk->read(_block_to_sector(group->block_usage_bitmap), sectors_per_block, bitmap);

            for (uint32_t i = 0; i < to_process; i++) {
                uint32_t bi = group_block_index + i;
                uint8_t* byte = &bitmap[bi / 8];
                uint8_t mask = 1 << (bi % 8);

                if (value) {
                    if ((*byte & mask) == 0) {
                        *byte |= mask;
                        group->unallocated_block_count--;
                    }
                } else {
                    if (*byte & mask) {
                        *byte &= ~mask;
                        group->unallocated_block_count++;
                    }
                }
            }

            // Write updated bitmap
            blk->write(_block_to_sector(group->block_usage_bitmap), sectors_per_block, bitmap);
            GlobalAllocator.FreePages(bitmap, size_in_pages);

            // Proceed to next group
            start_block += to_process;
            count -= to_process;
        }

        _save_block_group_descriptor(); // Save all modified group descriptors
        return true;
    }


    void ext2::_clear_block(uint32_t block){
        int size_in_pages = DIV_ROUND_UP(sectors_per_block * blk->block_size, 0x1000);
        void* buffer = GlobalAllocator.RequestPages(size_in_pages);
        memset(buffer, 0, size_in_pages * 0x1000);
        blk->write(_block_to_sector(block), sectors_per_block, buffer);
        GlobalAllocator.FreePages(buffer, size_in_pages);
    }

    void ext2::_inode_allocate_singly_indirect_block(ext2_inode* inode){
        uint32_t block = _find_free_block();
        _mark_block(block, true);
        _clear_block(block);
        inode->singly_indirect_block_pointer = block;
    }

    void ext2::_inode_allocate_double_indirect_block(ext2_inode* inode){
        uint32_t block = _find_free_block();
        _mark_block(block, true);
        _clear_block(block);
        inode->doubly_indirect_block_pointer = block;
    }

    void ext2::_inode_allocate_triple_indirect_block(ext2_inode* inode){
        uint32_t block = _find_free_block();
        _mark_block(block, true);
        _clear_block(block);
        inode->triply_indirect_block_pointer = block;
    }


    bool ext2::_inode_add_block_indirect_l1(uint32_t singly_indirect_block_pointer, uint32_t block){
        if (singly_indirect_block_pointer == 0) return false;

        uint32_t num_of_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));

        blk->read(_block_to_sector(singly_indirect_block_pointer), sectors_per_block, table);
        
        bool ret = false;

        for (int i = 0; i < num_of_entries; i++){
            uint32_t* ptr = &table[i];
            if (*ptr == 0 || *ptr == (uint32_t)-1){
                ret = true;
                *ptr = block;
                blk->write(_block_to_sector(singly_indirect_block_pointer), sectors_per_block, table);
                break;
            }
        }

        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));
        return ret;
    }

    bool ext2::_inode_add_block_indirect_l2(uint32_t doubly_indirect_block_pointer, uint32_t block){
        if (doubly_indirect_block_pointer == 0) return false;

        uint32_t num_of_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));

        blk->read(_block_to_sector(doubly_indirect_block_pointer), sectors_per_block, table);

        bool ret = false;

        for (int i = 0; i < num_of_entries; i++){
            uint32_t* ptr = &table[i];
            if (*ptr == 0 || *ptr == (uint32_t)-1){
                *ptr = _find_free_block();
                _mark_block(*ptr, true);
                _clear_block(*ptr);

                blk->write(_block_to_sector(doubly_indirect_block_pointer), sectors_per_block, table);
            }
            ret = _inode_add_block_indirect_l1(*ptr, block);
            if (ret) break;
        }
        
        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));
        return ret;
    }

    bool ext2::_inode_add_block_indirect_l3(uint32_t triple_indirect_block_pointer, uint32_t block){
        if (triple_indirect_block_pointer == 0) return false;

        uint32_t num_of_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));

        blk->read(_block_to_sector(triple_indirect_block_pointer), sectors_per_block, table);

        bool ret = false;

        for (int i = 0; i < num_of_entries; i++){
            uint32_t* ptr = &table[i];
            if (*ptr == 0 || *ptr == (uint32_t)-1){
                *ptr = _find_free_block();
                _mark_block(*ptr, true);
                _clear_block(*ptr);

                blk->write(_block_to_sector(triple_indirect_block_pointer), sectors_per_block, table);
            }
            ret = _inode_add_block_indirect_l2(*ptr, block);
            if (ret) break;
        }
        
        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));
        return ret;
    }

    void ext2::_inode_add_block(ext2_inode* inode, uint32_t block){
        for (int i = 0; i < 12; i++){
            uint32_t* ptr = &inode->direct_block_pointer[i];
            if (*ptr == 0 || *ptr == (uint32_t)-1){
                *ptr = block;
                return;
            }
        }
        if (inode->singly_indirect_block_pointer == 0 || inode->singly_indirect_block_pointer == (uint32_t)-1) _inode_allocate_singly_indirect_block(inode);

        bool ret = _inode_add_block_indirect_l1(inode->singly_indirect_block_pointer, block);
        if (ret) return;

        if (inode->doubly_indirect_block_pointer == 0 || inode->doubly_indirect_block_pointer == (uint32_t)-1) _inode_allocate_double_indirect_block(inode);
        ret = _inode_add_block_indirect_l2(inode->doubly_indirect_block_pointer, block);
        if (ret) return;

        if (inode->triply_indirect_block_pointer == 0 || inode->triply_indirect_block_pointer == (uint32_t)-1) _inode_allocate_triple_indirect_block(inode);
        ret = _inode_add_block_indirect_l3(inode->triply_indirect_block_pointer, block);
    }

    bool ext2::_inode_add_blocks_indirect_l1(uint32_t block_ptr, const uint32_t* blocks, uint32_t& block_index, uint32_t total) {
        if (block_ptr == 0 || block_index >= total) return false;

        uint32_t num_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));
        blk->read(_block_to_sector(block_ptr), sectors_per_block, table);

        bool updated = false;
        for (uint32_t i = 0; i < num_entries && block_index < total; i++) {
            if (table[i] == 0 || table[i] == (uint32_t)-1) {
                table[i] = blocks[block_index++];
                updated = true;
            }
        }

        if (updated)
            blk->write(_block_to_sector(block_ptr), sectors_per_block, table);

        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));
        return (block_index == total);
    }

    bool ext2::_inode_add_blocks_indirect_l2(uint32_t block_ptr, const uint32_t* blocks, uint32_t& block_index, uint32_t total) {
        if (block_ptr == 0 || block_index >= total) return false;

        uint32_t num_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));
        blk->read(_block_to_sector(block_ptr), sectors_per_block, table);

        for (uint32_t i = 0; i < num_entries && block_index < total; i++) {
            if (table[i] == 0 || table[i] == (uint32_t)-1) {
                uint32_t new_block = _find_free_block();
                if (!new_block) break;
                _mark_block(new_block, true);
                _clear_block(new_block);
                table[i] = new_block;
                blk->write(_block_to_sector(block_ptr), sectors_per_block, table);
            }

            _inode_add_blocks_indirect_l1(table[i], blocks, block_index, total);
        }

        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));
        return (block_index == total);
    }

    bool ext2::_inode_add_blocks_indirect_l3(uint32_t block_ptr, const uint32_t* blocks, uint32_t& block_index, uint32_t total) {
        if (block_ptr == 0 || block_index >= total) return false;

        uint32_t num_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));
        blk->read(_block_to_sector(block_ptr), sectors_per_block, table);

        for (uint32_t i = 0; i < num_entries && block_index < total; i++) {
            if (table[i] == 0 || table[i] == (uint32_t)-1) {
                uint32_t new_block = _find_free_block();
                if (!new_block) break;
                _mark_block(new_block, true);
                _clear_block(new_block);
                table[i] = new_block;
                blk->write(_block_to_sector(block_ptr), sectors_per_block, table);
            }

            _inode_add_blocks_indirect_l2(table[i], blocks, block_index, total);
        }

        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));
        return (block_index == total);
    }

    void ext2::_inode_add_blocks(ext2_inode* inode, const uint32_t* blocks, uint32_t count) {
        uint32_t index = 0;

        // Direct
        for (int i = 0; i < 12 && index < count; i++) {
            if (inode->direct_block_pointer[i] == 0 || inode->direct_block_pointer[i] == (uint32_t)-1) {
                inode->direct_block_pointer[i] = blocks[index++];
            }
        }

        // Singly indirect
        if (index < count) {
            if (inode->singly_indirect_block_pointer == 0 || inode->singly_indirect_block_pointer == (uint32_t)-1)
                _inode_allocate_singly_indirect_block(inode);
            _inode_add_blocks_indirect_l1(inode->singly_indirect_block_pointer, blocks, index, count);
        }

        // Doubly indirect
        if (index < count) {
            if (inode->doubly_indirect_block_pointer == 0 || inode->doubly_indirect_block_pointer == (uint32_t)-1)
                _inode_allocate_double_indirect_block(inode);
            _inode_add_blocks_indirect_l2(inode->doubly_indirect_block_pointer, blocks, index, count);
        }

        // Triply indirect (optional)
        if (index < count) {
            if (inode->triply_indirect_block_pointer == 0 || inode->triply_indirect_block_pointer == (uint32_t)-1)
                _inode_allocate_triple_indirect_block(inode);
            _inode_add_blocks_indirect_l3(inode->triply_indirect_block_pointer, blocks, index, count);
        }

        // Done
    }


    size_t ext2::_inode_truncate_indirect_l1(uint32_t singly_indirect_block_pointer, size_t block_count){
        if (singly_indirect_block_pointer == 0 || singly_indirect_block_pointer == (uint32_t)-1) return 0;

        uint32_t num_of_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));

        blk->read(_block_to_sector(singly_indirect_block_pointer), sectors_per_block, table);
        
        size_t cnt = 0;

        for (int i = 0; i < num_of_entries; i++){
            uint32_t* ptr = &table[i];
            if (cnt >= block_count) {
                if (*ptr == 0) continue;
                _mark_block(*ptr, false);
                *ptr = 0;
            }else cnt++;
        }

        blk->write(_block_to_sector(singly_indirect_block_pointer), sectors_per_block, table);
        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));
        return cnt;
    }

    size_t ext2::_inode_truncate_indirect_l2(uint32_t doubly_indirect_block_pointer, size_t block_count){
        if (doubly_indirect_block_pointer == 0 || doubly_indirect_block_pointer == (uint32_t)-1) return 0;

        uint32_t num_of_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));

        blk->read(_block_to_sector(doubly_indirect_block_pointer), sectors_per_block, table);

        size_t cnt = 0;

        for (int i = 0; i < num_of_entries; i++){
            uint32_t* ptr = &table[i];
            
            if (*ptr != 0){
                size_t c = _inode_truncate_indirect_l1(*ptr, block_count - cnt);
                cnt += c;
                if (c == 0){
                    _mark_block(*ptr, false);
                    *ptr = 0;
                }
            }
        }
        
        blk->write(_block_to_sector(doubly_indirect_block_pointer), sectors_per_block, table);
        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));
        return cnt;
    }

    size_t ext2::_inode_truncate_indirect_l3(uint32_t triple_indirect_block_pointer, size_t block_count){
        if (triple_indirect_block_pointer == 0 || triple_indirect_block_pointer == (uint32_t)-1) return 0;

        uint32_t num_of_entries = block_size / sizeof(uint32_t);
        uint32_t* table = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(block_size, 0x1000));

        blk->read(_block_to_sector(triple_indirect_block_pointer), sectors_per_block, table);

        size_t cnt = 0;

        for (int i = 0; i < num_of_entries; i++){
            uint32_t* ptr = &table[i];

            if (*ptr != 0){
                size_t c = _inode_truncate_indirect_l2(*ptr, block_count - cnt);
                cnt += c;
                if (c == 0){
                    _mark_block(*ptr, false);
                    *ptr = 0;
                }
            }
        }

        blk->write(_block_to_sector(triple_indirect_block_pointer), sectors_per_block, table);
        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));
        return cnt;
    }

    uint32_t ext2::inode_truncate(ext2_inode* inode, size_t num_of_blocks){
        size_t cnt = 0;
        for (int i = 0; i < 12; i++){
            uint32_t* ptr = &inode->direct_block_pointer[i];
            if (cnt >= num_of_blocks) {
                if (*ptr == 0 || *ptr == (uint32_t)-1) continue;
                _mark_block(*ptr, false);
                *ptr = 0;
            }else cnt++;
        }

        size_t c = _inode_truncate_indirect_l1(inode->singly_indirect_block_pointer, num_of_blocks - cnt);
        cnt += c;
        if (c == 0 && inode->singly_indirect_block_pointer != 0){
            _mark_block(inode->singly_indirect_block_pointer, false);
        }

        c = _inode_truncate_indirect_l2(inode->doubly_indirect_block_pointer, num_of_blocks - cnt);
        cnt += c;
        if (c == 0 && inode->doubly_indirect_block_pointer != 0){
            _mark_block(inode->doubly_indirect_block_pointer, false);
        }

        c = _inode_truncate_indirect_l3(inode->triply_indirect_block_pointer, num_of_blocks - cnt);
        cnt += c;
        if (c == 0 && inode->triply_indirect_block_pointer != 0){
            _mark_block(inode->triply_indirect_block_pointer, false);
        }

        inode->count_of_disk_sectors = cnt * sectors_per_block;
        return cnt * block_size;
    }

    bool ext2::_push_dir_entry(uint32_t parent_inode, ext2_directory* directory_entry){
        if (directory_entry->rec_len == 0) return false;
        if ((directory_entry->rec_len % 4) != 0) return false;

        ext2_inode* parent = read_inode(parent_inode);
        if (!parent) return false;

        uint64_t psize = ((uint64_t)parent->size_upper << 32) | parent->size_low;

        if (psize == 0) {
            // Empty directory case, just write the new entry as is
            bool success = write_inode(parent_inode, directory_entry, directory_entry->rec_len, 0);
            delete parent;
            return success;
        }

        void* current_data = GlobalAllocator.RequestPages(DIV_ROUND_UP(psize, 0x1000));
        if (!current_data) {
            delete parent;
            return false;
        }

        uint64_t length = load_inode(parent, current_data, psize, 0);
        if (length == 0) {
            GlobalAllocator.FreePages(current_data, DIV_ROUND_UP(psize, 0x1000));
            delete parent;
            return false;
        }

        uint64_t offset = 0;
        ext2_directory* last_entry = nullptr;

        while (offset < length) {
            ext2_directory* entry = (ext2_directory*)((uint8_t*)current_data + offset);
            if (entry->inode == 0 || entry->rec_len == 0) break;

            // Calculate the ideal length for this entry
            uint32_t ideal_len = ALIGN(sizeof(ext2_directory) + strlen(entry->name) + 1, 4);

            // Shrink rec_len of this entry if possible
            if (entry->rec_len > ideal_len) {
                entry->rec_len = ideal_len;
            }

            last_entry = entry;
            offset += entry->rec_len;
        }

        // Now offset points just after last valid entry.

        // Calculate remaining space in the last block after the last entry
        uint32_t block_offset = offset % block_size;
        uint32_t free_space_in_block = block_size - block_offset;

        if (directory_entry->rec_len > free_space_in_block) {
            // New entry too big to fit in the free space, fail or handle extending directory
            GlobalAllocator.FreePages(current_data, DIV_ROUND_UP(psize, 0x1000));
            delete parent;
            return false;
        }

        // Adjust last entry's rec_len to minimal size, so it doesn't consume the free space
        if (last_entry) {
            uint32_t last_entry_ideal_len = ALIGN(sizeof(ext2_directory) + strlen(last_entry->name) + 1, 4);
            last_entry->rec_len = last_entry_ideal_len;
        }

        // Set the new entry's rec_len to fill the remaining free space
        directory_entry->rec_len = free_space_in_block;

        // Allocate new buffer for updated directory data (offset + new entry size)
        void* new_data = malloc(offset + directory_entry->rec_len);
        if (!new_data) {
            GlobalAllocator.FreePages(current_data, DIV_ROUND_UP(psize, 0x1000));
            delete parent;
            return false;
        }

        memcpy_simd(new_data, current_data, offset);
        memcpy_simd((uint8_t*)new_data + offset, directory_entry, directory_entry->rec_len);

        GlobalAllocator.FreePages(current_data, DIV_ROUND_UP(psize, 0x1000));

        bool success = write_inode(parent_inode, new_data, offset + directory_entry->rec_len, 0);

        free(new_data);
        delete parent;

        return success;
    }


    int ext2::mkdir(uint32_t parent_inode, const char* name, ext2_directory** out){
        uint32_t inode_num = _find_free_inode();
        _mark_inode(inode_num, true); // reserve it

        ext2_inode* parent = read_inode(parent_inode);
        ext2_inode* inode = read_inode(inode_num);
        memset(inode, 0, inode_size_in_bytes);

        // Set perms (since the parent is also a directory, we can just copy its value)
        inode->type_perms = parent->type_perms;
        inode->user_id = parent->user_id;
        inode->group_id = parent->group_id;
        
        // Set time and date
        inode->creation_time = to_unix_timestamp(c_time->second, c_time->minute, c_time->hour, c_time->day, c_time->month, c_time->year);
        inode->last_access_time = inode->last_modification_time = inode->creation_time;

        // Other stuff
        inode->count_of_hard_links = 1;

        int pre_allocate = superblock->extended.blocks_to_preallocate_dir;
        if (pre_allocate == 0) pre_allocate = 1;

        for (int i = 0; i < pre_allocate; i++){
            uint32_t block = _find_free_block();
            _mark_block(block, true);
            _clear_block(block);
            _inode_add_block(inode, block);
            inode->count_of_disk_sectors += sectors_per_block;
        }

        int name_length = strlen(name);
        ext2_directory* entry = (ext2_directory*)malloc(ALIGN(sizeof(ext2_directory) + name_length, 4));
        entry->inode = inode_num;
        entry->name_length = name_length;
        entry->rec_len = sizeof(ext2_directory) + entry->name_length + 1;
        entry->rec_len = DIV_ROUND_UP(entry->rec_len, 4);
        entry->type_or_name = EXT2_DIRENT_DIR;
        memcpy(entry->name, (char*)name, name_length);


        int self_name_length = 1;
        ext2_directory* self = (ext2_directory*)malloc(ALIGN(sizeof(ext2_directory) + self_name_length, 4));
        self->inode = inode_num;
        self->name_length = self_name_length;
        self->rec_len = sizeof(ext2_directory) + self->name_length + 1;
        self->rec_len = DIV_ROUND_UP(self->rec_len, 4);

        self->type_or_name = EXT2_DIRENT_DIR;
        memcpy(self->name, ".", self_name_length);

        int parent_name_length = 2;
        ext2_directory* parent_dir = (ext2_directory*)malloc(ALIGN(sizeof(ext2_directory) + parent_name_length, 4));
        parent_dir->inode = parent_inode;
        parent_dir->name_length = parent_name_length;
        parent_dir->rec_len = sizeof(ext2_directory) + parent_dir->name_length + 1;
        parent_dir->rec_len = DIV_ROUND_UP(parent_dir->rec_len, 4);
        parent_dir->type_or_name = EXT2_DIRENT_DIR;
        memcpy(parent_dir->name, "..", parent_name_length);


        save_inode_entry(inode_num, inode);
        _push_dir_entry(parent_inode, entry);
        _push_dir_entry(inode_num, self);
        _push_dir_entry(inode_num, parent_dir);
        
        delete inode;
        delete parent;
        free(self);
        free(parent_dir);
        *out = entry;
        return 0;
    }

    int ext2::create_file(uint32_t parent_inode, const char* name, ext2_directory** out){
        uint32_t inode_num = _find_free_inode();
        _mark_inode(inode_num, true); // reserve it

        ext2_inode* parent = read_inode(parent_inode);
        ext2_inode* inode = read_inode(inode_num);
        memset(inode, 0, inode_size_in_bytes);

        // Set perms (since the parent is also a directory, we can just copy its value)
        inode->type_perms = (parent->type_perms & (~0xF000));
        inode->type_perms |= EXT2_REG;
        inode->user_id = parent->user_id;
        inode->group_id = parent->group_id;
        
        // Set time and date
        inode->creation_time = to_unix_timestamp(c_time->second, c_time->minute, c_time->hour, c_time->day, c_time->month, c_time->year);
        inode->last_access_time = inode->last_modification_time = inode->creation_time;

        // Other stuff
        inode->count_of_hard_links = 1;

        int pre_allocate = superblock->extended.blocks_to_preallocate;
        if (pre_allocate == 0) pre_allocate = 1;

        for (int i = 0; i < pre_allocate; i++){
            uint32_t block = _find_free_block();
            _mark_block(block, true);
            _clear_block(block);
            _inode_add_block(inode, block);
            inode->count_of_disk_sectors += sectors_per_block;
        }

        int name_length = strlen(name);
        ext2_directory* entry = (ext2_directory*)malloc(ALIGN(sizeof(ext2_directory) + name_length, 4));
        entry->inode = inode_num;
        entry->name_length = name_length;
        entry->rec_len = sizeof(ext2_directory) + entry->name_length + 1;
        entry->rec_len = DIV_ROUND_UP(entry->rec_len, 4);
        entry->type_or_name = EXT2_DIRENT_REG;
        memcpy(entry->name, (char*)name, name_length);


        save_inode_entry(inode_num, inode);
        _push_dir_entry(parent_inode, entry);
        
        delete inode;
        delete parent;
        *out = entry;
        return 0;
    }

    ext2_list* ext2::get_dir_cache(uint32_t dir){
        ext2_list* l = inode_cache;

        while (l != nullptr){
            if (l->parent_inode == dir) return l;
            l = l->next;
        }

        return nullptr;
    }

    void ext2::cache_vnode(uint32_t parent, vnode_t* snode){
        vnode_t* node = new vnode_t;
        memcpy(node, snode, sizeof(vnode_t));
        
        vnode_t* s = snode->next;
        vnode_t* c = node;
        while(s != nullptr){
            vnode_t* t = new vnode_t;
            memcpy(t, s, sizeof(vnode_t));

            c->next = t;
            c = t;
            s = s->next;
        }


        ext2_list* list = get_dir_cache(parent);
        if (list == nullptr){
            list = new ext2_list;
            list->child_list = nullptr;
            list->cnt = 10;
            list->parent_inode = parent;
            if (inode_cache != nullptr){
                ext2_list* last = inode_cache;
                while(last->next != nullptr) last = last->next;

                last->next = list;
            }else{
                inode_cache = list;
            }
        }

        if (list->child_list == 0){
            list->child_list = node;
        }else{
            append_child(list, node);
        }
    }

    void ext2::append_child(ext2_list* parent, vnode_t* node){
        if (parent->child_list == nullptr){
            parent->child_list = node;
            return;
        }

        vnode_t* last = parent->child_list;
        while(last->next != nullptr) last = last->next;

        last->next = node;
    }

    void ext2::clear_cache(uint32_t parent){
        ext2_list* list = get_dir_cache(parent);
        if (list == nullptr) return;

        vnode_t* node = list->child_list;
        while(node != nullptr){
            vnode_t* tmp = node;
            node = node->next;

            tmp->ref_cnt = 0;
            tmp->close();
        }
    }

    vnode_t* ext2::get_cached_children(uint32_t inode){
        ext2_list* list = get_dir_cache(inode);
        if (list == nullptr) return nullptr;

        vnode_t* start = nullptr;
        vnode_t* last = nullptr;
        vnode_t* c = list->child_list;

        while(c != nullptr){
            vnode_t* t = new vnode_t;
            memcpy(t, c, sizeof(vnode_t));
            t->next = nullptr;
            if (last) last->next = t;
            if (!start) start = t;
            
            last = t;
            c = c->next;
        }

        return start;
    }
}