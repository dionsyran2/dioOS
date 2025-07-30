#include <filesystem/ext2/ext2.h>
#include <paging/PageFrameAllocator.h>
#include <memory.h>
#include <BasicRenderer.h>
#include <drivers/rtc/rtc.h>
#include <scheduling/apic/apic.h>
#include <math.h>
#include <kerrno.h>

namespace filesystem{

    void* ext2_def_read(size_t* cnt, vnode_t* this_node){
        ext2* fs = (ext2*)this_node->misc_data[0];

        uint32_t c = 0;
        ext2_inode* inode = fs->read_inode(this_node->inode);
        serialf("\e[0;35mLoading %s\e[0m\n", this_node->name);
        void* ret = fs->load_inode(inode, &c);
        *cnt = c;

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

    int ext2_write(const char* data, size_t length, vnode* this_node){
        ext2* fs = (ext2*)this_node->misc_data[0];
        bool ret = fs->write_inode(this_node->inode, (void*)data, length);
        if (ret == false) return -EFAULT;
        this_node->size = length;
        return 0;
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
        uint64_t sector_offset = block * block_size;
        sector_offset /= blk->block_size;
        return start_sector + sector_offset;
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
            if (ptr != 0) count++;
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
            if (ptr != 0) count += _number_of_blocks_indirect_l1(ptr);
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
            if (ptr != 0) count += _number_of_blocks_indirect_l2(ptr);
        }
        
        GlobalAllocator.FreePages(table, DIV_ROUND_UP(block_size, 0x1000));

        return count;
    }

    uint32_t ext2::_number_of_blocks_direct(ext2_inode* inode){
        if (inode == nullptr) return 0;
        uint32_t count = 0;

        for (int i = 0; i < 12; i++){
            uint32_t ptr = inode->direct_block_pointer[i];
            if (ptr != 0) count++;
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
            if (ptr == 0) continue;
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
            if (ptr == 0) continue;
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
            if (ptr != 0) _push_block_table_indirect_l1(ptr, out, offset);
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
            if (ptr != 0) _push_block_table_indirect_l2(ptr, out, offset);
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

    bool ext2::_save_inode_entry(uint32_t inode, ext2_inode* inode_entry){
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

    void* ext2::load_inode(ext2_inode* inode, uint32_t* ln){
        uint32_t length = 0;
        uint32_t* blocks = _get_block_table(inode, &length);
        void* mem = GlobalAllocator.RequestPages(DIV_ROUND_UP(length * block_size, 0x1000));
        size_t offset = 0;

        for (uint32_t block = 0; block < length; block++){
            uint32_t blk_ptr = blocks[block];
            uint32_t num_of_blks = 1;
            uint32_t prev = blk_ptr;

            for (uint32_t b = block + 1; b < length; b++){
                if (blocks[b] != prev + 1) break;
                prev = blocks[b];
                num_of_blks++;
            }

            uint32_t bytes = num_of_blks * block_size;
            uint32_t sectors = DIV_ROUND_UP(bytes, blk->block_size);
            uint32_t sector = _block_to_sector(blk_ptr);

            void* tmp = GlobalAllocator.RequestPages(DIV_ROUND_UP(sectors * blk->block_size, 0x1000));
            blk->read(sector, sectors, tmp);
            memcpy((uint8_t*)mem + offset, tmp, bytes);
            GlobalAllocator.FreePages(tmp, DIV_ROUND_UP(sectors * blk->block_size, 0x1000));
            offset += bytes;
            block += (num_of_blks - 1);
        }

        delete[] blocks;

        void* heap = malloc(offset/* + 0x1000*/);
        //heap += (0x1000 - ((uint64_t)heap & 0xFFF)); yeah... this is a bad idea, lets not do that
        memcpy(heap, mem, offset);
        GlobalAllocator.FreePages(mem, DIV_ROUND_UP(length * block_size, 0x1000));
        *ln = inode->size_low;

        return heap;
    }

    bool ext2::write_inode(uint32_t inode_num, void* buff, uint32_t buffer_length){
        ext2_inode* inode = read_inode(inode_num);

        uint32_t length = 0;
        uint32_t* blocks = _get_block_table(inode, &length);
        if ((length * block_size) < buffer_length){
            while ((length * block_size) < buffer_length){
                uint32_t block = _find_free_block();
                _mark_block(block, true);
                _inode_add_block(inode, block);
                length ++;
            }

            delete[] blocks;
            blocks = _get_block_table(inode, &length);
        }

        void* mem = GlobalAllocator.RequestPages(DIV_ROUND_UP(buffer_length, 0x1000));
        memset(mem, 0, DIV_ROUND_UP(buffer_length, 0x1000) * 0x1000);
        memcpy_simd(mem, buff, buffer_length);

        size_t offset = 0;

        for (uint32_t block = 0; block < length; block++){
            uint32_t blk_ptr = blocks[block];
            uint32_t num_of_blks = 1;
            uint32_t prev = blk_ptr;

            for (uint32_t b = block + 1; b < length; b++){
                if (blocks[b] != prev + 1) break;
                prev = blocks[b];
                num_of_blks++;
            }

            uint32_t bytes = num_of_blks * block_size;
            uint32_t sectors = DIV_ROUND_UP(bytes, blk->block_size);
            uint32_t sector = _block_to_sector(blk_ptr);

            if (!blk->write(sector, sectors, (uint8_t*)mem + offset)) return false;

            offset += bytes;
            block += (num_of_blks - 1);
        }

        delete[] blocks;

        // metadata
        inode->last_access_time = inode->last_modification_time = to_unix_timestamp(c_time);
        inode->size_low = buffer_length;
        _save_inode_entry(inode_num, inode);

        delete inode;
        
        GlobalAllocator.FreePages(mem, DIV_ROUND_UP(buffer_length, 0x1000));

        return true;
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
        node->size = ((uint64_t)inode->size_upper << 32) | inode->size_low;

        
        node->ops.read = ext2_def_read;
        node->ops.mkdir = ext2_mkdir;
        node->ops.creat = ext2_creat;
        node->ops.write = ext2_write;

        node->misc_data[0] = (void*)this;

        node->inode = dir->inode;
        node->gid = inode->group_id;
        node->uid = inode->user_id;
        node->perms = inode->type_perms & 0xFFF;
        node->creation_time = inode->creation_time;
        node->last_accessed = inode->last_access_time;
        node->last_modified = inode->last_modification_time;
        node->size = inode->size_low;
        
        delete name;
        return node;
    }

    void ext2::mount_dir(uint32_t inode, vnode_t* parent, bool sub){
        ext2_inode* ind = read_inode(inode);

        uint32_t length = 0;
        uint64_t fsz = ind->size_low | ((uint64_t)ind->size_upper << 32);
        void* data = load_inode(ind, &length);

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

        mount_dir(2, fs, true);
        return vfs::get_full_path_name(fs);
    }

    uint32_t ext2::_find_free_inode(){
        for (size_t i = 0; i < block_group_count; i++){
            ext2_block_group_descriptor* group = &block_group_table[i];

            if (group->unallocated_inode_count == 0) continue;

            int size_in_pages = DIV_ROUND_UP(sectors_per_block * blk->block_size, 0x1000);

            uint8_t* bitmap = (uint8_t*)GlobalAllocator.RequestPages(size_in_pages);
            blk->read(_block_to_sector(group->inode_usage_bitmap), sectors_per_block, bitmap);

            for (int b = superblock->extended.first_non_reserved_inode; b < inodes_per_group; b++){

                uint8_t byte = bitmap[b / 8];
                uint8_t mask = 1 << (b % 8);

                if ((byte & mask) == 0){
                    GlobalAllocator.FreePages(bitmap, size_in_pages);
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
        for (size_t i = 0; i < block_group_count; i++){
            ext2_block_group_descriptor* group = &block_group_table[i];

            if (group->unallocated_block_count == 0) continue;

            int size_in_pages = DIV_ROUND_UP(sectors_per_block * blk->block_size, 0x1000);

            uint8_t* bitmap = (uint8_t*)GlobalAllocator.RequestPages(size_in_pages);
            blk->read(_block_to_sector(group->block_usage_bitmap), sectors_per_block, bitmap);

            for (int b = 0; b < superblock->num_of_blocks_in_group; b++){

                uint8_t byte = bitmap[b / 8];
                uint8_t mask = 1 << (b % 8);

                if ((byte & mask) == 0){
                    GlobalAllocator.FreePages(bitmap, size_in_pages);
                    return (i * superblock->num_of_blocks_in_group) + b;
                }
            }

            GlobalAllocator.FreePages(bitmap, size_in_pages);
        }
        return 0;
    }

    void ext2::_clear_block(uint32_t block){
        int size_in_pages = DIV_ROUND_UP(sectors_per_block * blk->block_size, 0x1000);
        void* buffer = GlobalAllocator.RequestPages(size_in_pages);
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
            if (*ptr == 0){
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
            if (*ptr == 0){
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
            if (*ptr == 0){
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
            if (*ptr == 0){
                *ptr = block;
                return;
            }
        }
        if (inode->singly_indirect_block_pointer == 0) _inode_allocate_singly_indirect_block(inode);

        bool ret = _inode_add_block_indirect_l1(inode->singly_indirect_block_pointer, block);
        if (ret) return;

        if (inode->doubly_indirect_block_pointer == 0) _inode_allocate_double_indirect_block(inode);
        ret = _inode_add_block_indirect_l2(inode->doubly_indirect_block_pointer, block);
        if (ret) return;

        if (inode->triply_indirect_block_pointer == 0) _inode_allocate_triple_indirect_block(inode);
        ret = _inode_add_block_indirect_l3(inode->triply_indirect_block_pointer, block);
    }


    size_t ext2::_inode_truncate_indirect_l1(uint32_t singly_indirect_block_pointer, size_t block_count){
        if (singly_indirect_block_pointer == 0) return 0;

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
        if (doubly_indirect_block_pointer == 0) return 0;

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
        if (triple_indirect_block_pointer == 0) return 0;

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

    uint32_t ext2::_inode_truncate(ext2_inode* inode, size_t num_of_blocks){
        size_t cnt = 0;
        for (int i = 0; i < 12; i++){
            uint32_t* ptr = &inode->direct_block_pointer[i];
            if (cnt >= num_of_blocks) {
                if (*ptr == 0) continue;
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
        return cnt;
    }


    bool ext2::_mark_block(uint32_t block, bool v){
        uint32_t block_group_index = block / superblock->num_of_blocks_in_group;
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

    bool ext2::_push_dir_entry(uint32_t parent_inode, ext2_directory* directory_entry){
        if (directory_entry->rec_len == 0) return false;
        ext2_inode* parent = read_inode(parent_inode);
        uint32_t length = 0;
        void* current_data = load_inode(parent, &length);

        void* new_data = malloc(length + directory_entry->rec_len);
        memcpy_simd(new_data, current_data, length);
        memcpy_simd((uint8_t*)new_data + length, directory_entry, directory_entry->rec_len);

        free(current_data);

        if (!write_inode(parent_inode, new_data, length + directory_entry->rec_len)) return false;
        return true;
    }

    int ext2::mkdir(uint32_t parent_inode, const char* name, ext2_directory** out){
        uint32_t inode_num = _find_free_inode();
        _mark_inode(inode_num, true); // reserve it
        serialf("inode: %d\n", inode_num);

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
        entry->type_or_name = EXT2_DIRENT_DIR;
        memcpy(entry->name, (char*)name, name_length);


        int self_name_length = 1;
        ext2_directory* self = (ext2_directory*)malloc(ALIGN(sizeof(ext2_directory) + self_name_length, 4));
        self->inode = inode_num;
        self->name_length = self_name_length;
        self->rec_len = sizeof(ext2_directory) + self->name_length + 1;
        self->type_or_name = EXT2_DIRENT_DIR;
        memcpy(self->name, ".", self_name_length);

        int parent_name_length = 2;
        ext2_directory* parent_dir = (ext2_directory*)malloc(ALIGN(sizeof(ext2_directory) + parent_name_length, 4));
        parent_dir->inode = parent_inode;
        parent_dir->name_length = parent_name_length;
        parent_dir->rec_len = sizeof(ext2_directory) + parent_dir->name_length + 1;
        parent_dir->type_or_name = EXT2_DIRENT_DIR;
        memcpy(parent_dir->name, "..", parent_name_length);


        _save_inode_entry(inode_num, inode);
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
        entry->type_or_name = EXT2_DIRENT_REG;
        memcpy(entry->name, (char*)name, name_length);


        _save_inode_entry(inode_num, inode);
        _push_dir_entry(parent_inode, entry);
        
        delete inode;
        delete parent;
        *out = entry;
        return 0;
    }
}