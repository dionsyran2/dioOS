#include <filesystem/ext2/ext2.h>
#include <memory/heap.h>
#include <kstdio.h>
#include <drivers/timers/common.h>
#include <math.h>
#include <kerrno.h>
#include <cstr.h>

void ext2_on_mount(vnode_t* this_node){
    filesystems::ext2_t* fs = (filesystems::ext2_t*)this_node->fs_identifier;
    fs->mount();
}

// TO-DO: int (*unlink)(vnode_t* this_node);
// TO-DO: int (*rmdir)(vnode_t* this_node);
extern vnode_ops_t ext2_dir_ops;
extern vnode_ops_t ext2_file_ops;

vnode_type_t get_inode_type(ext2_inode_t* inode){
    uint16_t type = inode->type_permissions & 0xF000;

    if (type == EXT2_INODE_FIFO) return VPIPE;
    if (type == EXT2_INODE_CHR)  return VCHR;
    if (type == EXT2_INODE_DIR)  return VDIR;
    if (type == EXT2_INODE_BLK)  return VBLK;
    if (type == EXT2_INODE_REG)  return VREG;
    if (type == EXT2_INODE_SYM)  return VLNK;
    if (type == EXT2_INODE_SOC)  return VSOC;

    return VREG; 
}

int ext2_vnode_read_dir(vnode_t** out, vnode_t* this_node){
    if (this_node->type != VDIR) return -ENOTDIR;

    filesystems::ext2_t* fs = (filesystems::ext2_t*)this_node->fs_identifier;
    int node_count = 0;
    vnode_t* node_list = nullptr;


    uint64_t chunk_size = 4096;
    uint8_t* buffer = (uint8_t*)malloc(chunk_size);
    uint64_t file_offset = 0;

    while (true) {
        // Read a chunk of the directory file
        int64_t bytes_read = fs->load_inode_contents(this_node->inode, file_offset, chunk_size, buffer);
        
        // Stop if EOF (read 0 bytes) or Error (< 0)
        if (bytes_read <= 0) break;

        uint64_t buffer_pos = 0;
        
        while (buffer_pos < bytes_read) {
            // Get pointer to current entry
            ext2_directory_entry_t* entry = (ext2_directory_entry_t*)(buffer + buffer_pos);

            // Corrupted filesystem loop prevention
            if (entry->entry_size == 0) break; 

            // Process valid entries
            if (entry->inode != 0) {
                uint32_t len = entry->name_size_low; 
                
                // Check the type and create a vnode entry
                ext2_inode_t* inode = fs->fetch_inode(entry->inode);
                if (inode){
                    vnode_type_t type = get_inode_type(inode);

                    vnode_t* vnode = new vnode_t(type);
                    uint32_t name_len = min(len, sizeof(vnode->name) - 1);
                    memcpy(vnode->name, entry->name, name_len);
                    vnode->name[name_len] = '\0';

                    vnode->fs_identifier = this_node->fs_identifier;
                    vnode->inode = entry->inode;
                    vnode->size = fs->get_inode_size(inode);
                    
                    vnode->permissions = inode->type_permissions & 0xFFF;
                    vnode->uid = inode->owner_uid;
                    vnode->gid = inode->owner_gid;
                    vnode->last_accessed = inode->last_access_time;
                    vnode->last_modified = inode->last_modification_time;
                    vnode->creation_time = inode->change_time;
                    vnode->next = nullptr;
                    vnode->parent = this_node;
                    
                    if (type == VDIR){
                        memcpy(&vnode->file_operations, &ext2_dir_ops, sizeof(vnode->file_operations));
                    }else {
                        memcpy(&vnode->file_operations, &ext2_file_ops, sizeof(vnode->file_operations));
                    }

                    if (node_list == nullptr){
                        node_list = vnode;
                    } else {
                        vnode_t* c = node_list;
                        while (c->next) c = c->next;

                        c->next = vnode;
                    }
                    node_count++;

                    delete inode;
                }
            }

            // Advance position inside the buffer
            buffer_pos += entry->entry_size;
        }

        // Advance the global file offset by what we actually read
        file_offset += bytes_read;
    }

    free(buffer);

    *out = node_list;
    return node_count;
}

namespace filesystems{
    bool is_ext2(vnode_t* blk){
        // Check the superblock
        ext2_superblock_t* sblock = new ext2_superblock_t;
        if (!blk->read(EXT2_SUPERBLOCK_OFFSET, sizeof(ext2_superblock_t), sblock)){
            delete sblock;
            return false;
        }

        bool is_ext2 = sblock->signature == EXT2_SIGNATURE;
        delete sblock;

        return is_ext2;
    };

    ext2_t::ext2_t(vnode_t* blk){
        memset(this, 0, sizeof(ext2_t));

        this->blk = blk;
        this->mounted = false;

        this->superblock = new ext2_superblock_t;
        this->extended_superblock = new ext2_extended_superblock_t;
        block_group_table = nullptr; // To be allocated
    }

    ext2_t::~ext2_t(){
        delete this->superblock;
        delete this->extended_superblock;

        if (block_group_table) free(block_group_table);
    }

    bool ext2_t::mount(){
        if (this->mounted) {
            __atomic_add_fetch(&this->ref_count, 1, __ATOMIC_SEQ_CST);
            return true;
        }

        if (!this->_load_superblock()) {
            kprintf("[EXT2] Could not load the superblock\n");
            return false;
        }

        if (this->superblock->major_version < 1) {
            kprintf("[EXT2] Version %d.%d not supported!\n",
                this->superblock->major_version, this->superblock->minor_version);
            return false;
        }

        if (!this->_load_extended_superblock()) {
            kprintf("[EXT2] Could not load the extended superblock\n");
            return false;
        }

        // Check required features
        if (this->extended_superblock->required_features & 0b101){
            kprintf("[EXT2] Required features are not supported!\n");
            return false;
        }

        // Calculate the block size
        this->block_size = 1024 << this->superblock->log2_block_size;

        if (!this->_load_block_group_table()) {
            kprintf("[EXT2] Could not load the block group table\n");
            return false;
        }

        // Update the last mount time & times mounted
        this->superblock->last_mount_time = current_time;
        this->superblock->mount_count++;

        // Write changes to the superblock
        _save_superblock();

        this->ref_count = 1;
        this->mounted = true;

        // Update size informaton
        this->root->partition_total_size = _get_total_size();
        this->root->size = _calculate_used_space();

        return true;
    }

    bool ext2_t::_load_superblock(){
        return blk->read(EXT2_SUPERBLOCK_OFFSET, sizeof(ext2_superblock_t), this->superblock) == sizeof(ext2_superblock_t);
    }

    bool ext2_t::_save_superblock(){
        return blk->write(EXT2_SUPERBLOCK_OFFSET, sizeof(ext2_superblock_t), this->superblock) == sizeof(ext2_superblock_t);
    }

    bool ext2_t::_load_extended_superblock(){
        return blk->read(EXT2_SUPERBLOCK_OFFSET + sizeof(ext2_superblock_t), sizeof(ext2_extended_superblock_t), this->extended_superblock)
            == sizeof(ext2_extended_superblock_t);
    }

    bool ext2_t::_save_extended_superblock(){
        return blk->write(EXT2_SUPERBLOCK_OFFSET + sizeof(ext2_superblock_t), sizeof(ext2_extended_superblock_t), this->extended_superblock)
            == sizeof(ext2_extended_superblock_t);
    }

    bool ext2_t::_load_block_group_table(){
        // Calculate the total block_group_count
        this->block_group_count = ROUND_UP(this->superblock->total_block_count, this->superblock->blocks_per_block_group)
                / this->superblock->blocks_per_block_group;

        uint64_t block_group_size = sizeof(ext2_block_group_descriptor_t) * this->block_group_count;

        if (this->block_group_table == nullptr){
            this->block_group_table = (ext2_block_group_descriptor_t*)malloc(block_group_size);
        }

        uint32_t block = block_size == 1024 ? 2 : 1;

        return blk->read(_block_to_offset(block), block_group_size, this->block_group_table) == block_group_size;
    }

    bool ext2_t::_save_block_group_table(){
        uint64_t block_group_size = sizeof(ext2_block_group_descriptor_t) * this->block_group_count;
        uint32_t block = block_size == 1024 ? 2 : 1;

        return blk->write(_block_to_offset(block), block_group_size, this->block_group_table) == block_group_size;
    }

    
    uint64_t ext2_t::_get_total_size() {
        uint64_t total_blocks = this->superblock->total_block_count;

        return total_blocks * this->block_size;
    }

    uint64_t ext2_t::_calculate_used_space(){
        uint32_t total_blocks = this->superblock->total_block_count;
        uint32_t free_blocks = this->superblock->unallocated_block_count;

        uint64_t used_blocks = (uint64_t)(total_blocks - free_blocks);

        return used_blocks * this->block_size;
    }

    vnode_t* ext2_t::create_root_node(){
        if (this->root) return this->root;

        this->root = new vnode_t(VDIR);
        strcpy(root->name, "EXT2_ROOT_NODE");

        memcpy(&this->root->file_operations, &ext2_dir_ops, sizeof(this->root->file_operations));
        this->root->file_operations.mount = ext2_on_mount;

        this->root->fs_identifier = (uint64_t)this;
        this->root->inode = EXT2_ROOT_INODE;
        
        return this->root;
    }

    

} // namespace filesystems