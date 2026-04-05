#include <filesystem/ext2/ext2.h>
#include <memory/heap.h>
#include <kstdio.h>
#include <drivers/timers/common.h>
#include <math.h>
#include <kerrno.h>
#include <cstr.h>



char *ext2_vnode_read_link(vnode_t* this_node){
    if (this_node->type != VLNK) return nullptr;

    filesystems::ext2_t* fs = (filesystems::ext2_t*)this_node->fs_identifier;

    ext2_inode_t* inode = fs->fetch_inode(this_node->inode);

    uint64_t size = fs->get_inode_size(inode);

    /* If the size is less or equal to 60, It will use the
    *  direct block pointer array to save the path, in order to save space.
    *  Otherwise it will store it just like any other file                   */

    char *data = (char *)malloc(size);
    if (size <= 60){
        memcpy(data, inode->direct_block_pointer, size);
        data[size] = '\0';
    }else{
        fs->load_inode_contents(this_node->inode, 0, size, data);
        data[size] = '\0';
    }
    
    return data;
}


int ext2_vnode_mkdir(char* name, vnode_t* this_node){
    if (this_node->type != VDIR) return -ENOTDIR;

    filesystems::ext2_t* fs = (filesystems::ext2_t*)this_node->fs_identifier;

    int ret = fs->create_directory(this_node->inode, name);

    ext2_inode_t* self = fs->fetch_inode(this_node->inode);
    this_node->size = fs->get_inode_size(self);

    delete self;
    return ret;
}

int ext2_vnode_creat(char* name, vnode_t* this_node){
    if (this_node->type != VDIR) return -ENOTDIR;

    filesystems::ext2_t* fs = (filesystems::ext2_t*)this_node->fs_identifier;

    int ret = fs->create_file(this_node->inode, name);

    ext2_inode_t* self = fs->fetch_inode(this_node->inode);
    this_node->size = fs->get_inode_size(self);

    delete self;
    return ret;
}

int ext2_vnode_read_dir(vnode_t** out, vnode_t* this_node);

int ext2_vnode_read(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    if (this_node->type == VDIR) return -EISDIR;

    filesystems::ext2_t* fs = (filesystems::ext2_t*)this_node->fs_identifier;

    return fs->load_inode_contents(this_node->inode, offset, length, buffer);
}

int ext2_vnode_write(uint64_t offset, uint64_t length, const void* buffer, vnode_t* this_node){
    if (this_node->type == VDIR) return -EISDIR;

    filesystems::ext2_t* fs = (filesystems::ext2_t*)this_node->fs_identifier;
    this_node->last_modified = current_time;

    int ret = fs->write_inode_contents(this_node->inode, offset, length, buffer);

    ext2_inode_t* self = fs->fetch_inode(this_node->inode);
    this_node->size = fs->get_inode_size(self);

    delete self;

    return ret;
}

int ext2_vnode_truncate(uint64_t new_size, vnode_t* this_node){
    if (this_node->type == VDIR) return -EISDIR;

    filesystems::ext2_t* fs = (filesystems::ext2_t*)this_node->fs_identifier;
    this_node->last_modified = current_time;

    return fs->truncate_inode(this_node->inode, new_size);
}

int ext2_vnode_unlink(vnode_t* this_node){
    if (this_node->type == VDIR) return -EISDIR;

    filesystems::ext2_t* fs = (filesystems::ext2_t*)this_node->fs_identifier;

    ext2_inode_t* inode = fs->fetch_inode(this_node->inode);
    if (!inode) return -EFAULT;

    inode->hard_link_count--;

    fs->update_inode(this_node->inode, inode);
    if (this_node->parent)
            fs->directory_remove_entry(this_node->parent->inode, this_node->name);

    if (inode->hard_link_count == 0)
        fs->free_inode_contents(this_node->inode);

    return 0;
}
int ext2_vnode_rmdir(vnode_t* this_node){
    if (this_node->type != VDIR) return -ENOTDIR;
    if (this_node->inode == 2) return -EFAULT; // Protect Root

    filesystems::ext2_t* fs = (filesystems::ext2_t*)this_node->fs_identifier;

    // Check for Empty Directory
    bool found_other_files = false;
    uint64_t chunk_size = 4096;
    uint8_t* buffer = (uint8_t*)malloc(chunk_size);
    uint64_t file_offset = 0;

    while (true) {
        int64_t bytes_read = fs->load_inode_contents(this_node->inode, file_offset, chunk_size, buffer);
        if (bytes_read <= 0) break;

        uint64_t buffer_pos = 0;
        while (buffer_pos < bytes_read) {
            ext2_directory_entry_t* entry = (ext2_directory_entry_t*)(buffer + buffer_pos);
            if (entry->entry_size == 0) break;
            
            // Skip empty slots (inode 0)
            if (entry->inode != 0) {
                // Check if name is "." or ".."
                bool is_dot  = (entry->name_size_low == 1 && entry->name[0] == '.');
                bool is_dot2 = (entry->name_size_low == 2 && entry->name[0] == '.' && entry->name[1] == '.');

                if (!is_dot && !is_dot2) {
                    found_other_files = true;
                    break;
                }
            }
            buffer_pos += entry->entry_size;
        }
        if (found_other_files) break;
        file_offset += bytes_read;
    }
    free(buffer);
    
    if (found_other_files) return -ENOTEMPTY;

    // Remove the entry from the parent
    if (this_node->parent) {
        fs->directory_remove_entry(this_node->parent->inode, this_node->name);
        
        // Decrement parent link count (Removing the ".." reference)
        ext2_inode_t* parent_inode = fs->fetch_inode(this_node->parent->inode);
        if (parent_inode) {
            if (parent_inode->hard_link_count > 0) parent_inode->hard_link_count--;
            fs->update_inode(this_node->parent->inode, parent_inode);
        }
    }

    // Destroy the directory inode
    ext2_inode_t* dir_inode = fs->fetch_inode(this_node->inode);
    if (dir_inode) {
        dir_inode->hard_link_count = 0;
        fs->update_inode(this_node->inode, dir_inode);
    }
    fs->free_inode_contents(this_node->inode);

    return 0;
}

void ext2_save_metadata(vnode_t* this_node){
    filesystems::ext2_t* fs = (filesystems::ext2_t*)this_node->fs_identifier;

    ext2_inode_t* inode = fs->fetch_inode(this_node->inode);
    if (!inode) return;

    inode->owner_uid = this_node->uid;
    inode->owner_gid = this_node->gid;
    inode->type_permissions &= 0xF000;
    inode->type_permissions |= (this_node->permissions & 0x0FFF);

    fs->update_inode(this_node->inode, inode);
    delete inode;
}

int ext2_vnode_find_file(const char *filename, vnode_t** out, vnode_t* this_node);

vnode_ops_t ext2_dir_ops = {
    .read_dir = ext2_vnode_read_dir,
    .find_file = ext2_vnode_find_file,
    .mkdir = ext2_vnode_mkdir,
    .creat = ext2_vnode_creat,
    .rmdir = ext2_vnode_rmdir,
    .save_metadata = ext2_save_metadata,
};

vnode_ops_t ext2_file_ops = {
    .read_link = ext2_vnode_read_link,
    .read = ext2_vnode_read,
    .write = ext2_vnode_write,
    .truncate = ext2_vnode_truncate,
    .unlink = ext2_vnode_unlink,
    .save_metadata = ext2_save_metadata,
};