#include <filesystem/ext2/ext2.h>
#include <paging/PageFrameAllocator.h>
#include <memory.h>
#include <BasicRenderer.h>
#include <drivers/rtc/rtc.h>
#include <scheduling/apic/apic.h>
#include <math.h>

namespace filesystem{

    void* ext2_def_load(size_t* cnt, vnode_t* this_node){
        ext2* fs = (ext2*)this_node->fs_data;
        ext2_directory* dir = (ext2_directory*)this_node->fs_sec_data;
        uint32_t c = 0;
        ext2_inode* inode = fs->_read_inode(dir->inode);
        void* ret = fs->_load_inode(inode, &c);
        *cnt = c;
        delete inode;
        return ret;
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

    uint32_t ext2::_number_of_blocks_indirect_l1(uint32_t singly_indirect_block_pointer){
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

    ext2_inode* ext2::_read_inode(uint32_t inode){
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

    void* ext2::_load_inode(ext2_inode* inode, uint32_t* ln){
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

    VNODE_TYPE ext2_type_to_vfs(uint32_t type){
        if (type & EXT2_FIFO) return VPIPE;
        if (type & EXT2_CHR) return VCHR;
        if (type & EXT2_DIR) return VDIR;
        if (type & EXT2_BLK) return VBLK;
        if (type & EXT2_REG) return VREG;
        if (type & EXT2_SYM) return VLNK;
        if (type & EXT2_SOC) return VSOC;
        return VBAD;
    }

    vnode_t* ext2::_create_and_mount_dir(ext2_directory* dir, ext2_inode* inode, vnode_t* parent){
        char* name = new char[dir->name_length + 1];
        memcpy(name, dir->name, dir->name_length);

        vnode_t* node = vfs::mount_node(name, ext2_type_to_vfs(inode->type_perms), parent);        
        node->size = ((uint64_t)inode->size_upper << 32) | inode->size_low;

        
        node->ops.load = ext2_def_load;
        node->fs_data = (void*)this;
        
        ext2_directory* copy = (ext2_directory*)malloc(dir->rec_len);
        memcpy(copy, dir, dir->rec_len);
        node->fs_sec_data = copy;
        delete name;
        return node;
    }

    void ext2::_mount_dir(uint32_t inode, vnode_t* parent, bool sub){
        ext2_inode* ind = _read_inode(inode);

        uint32_t length = 0;
        uint64_t fsz = ind->size_low | ((uint64_t)ind->size_upper << 32);
        void* data = _load_inode(ind, &length);

        size_t offset = 0;
        while (offset < fsz){
            ext2_directory* entry = (ext2_directory*)((uint8_t*)data + offset);
            char name[entry->name_length + 1] = { 0 };
            memcpy(&name, entry->name, entry->name_length);

            if (entry->inode == 0 || entry->rec_len == 0) break;

            if (strcmp(".", name) == 0 || strcmp("..", name) == 0) {offset += entry->rec_len; continue;}

            ext2_inode* ind = _read_inode(entry->inode);
            
            vnode_t* node = _create_and_mount_dir(entry, ind, parent);
            if (ind->type_perms & EXT2_DIR && sub){
                _mount_dir(entry->inode, node, sub);
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
        fs->fs_data = (void*)this;
        //fs->ops.create_subdirectory = fat32_create_dir;*/

        _mount_dir(2, fs, true);
        return vfs::get_full_path_name(fs);
    }
}