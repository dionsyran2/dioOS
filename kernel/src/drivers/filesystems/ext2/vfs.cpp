#include <drivers/filesystems/ext2/ext2.h>
#include <drivers/filesystems/common.h>
#include <vfs/vfs.h>
#include <drivers/drivers.h>
#include <kstdio.h>
#include <drivers/timers/common.h>
#include <memory.h>
#include <memory/heap.h>
#include <kerrno.h>
#include <cstr.h>
#include <bits/poll.h>

namespace filesystems {
    extern vnode_file_operations_t ext2_file_operations;

    vnode_t *ext2_fs_fetch_vnode(dentry_t *entry){
        filesystems::ext2_t* fs = (filesystems::ext2_t*)entry->fs_data;
        ext2_inode_t *inode = fs->fetch_inode(entry->inode);

        vnode_t *ret = new vnode_t();
        ret->operations = &ext2_file_operations;
        ret->nlink = inode->hard_link_count;

        ret->attributes.valid = (
            VNODE_ATTR_MODE | VNODE_ATTR_UID | VNODE_ATTR_GID |
            VNODE_ATTR_MTIME | VNODE_ATTR_CTIME
        );
        ret->attributes.mode = inode->mode;
        ret->attributes.uid = inode->owner_uid;
        ret->attributes.gid = inode->owner_gid;
        ret->attributes.atime = inode->last_access_time;
        ret->attributes.mtime = inode->last_modification_time;
        ret->attributes.ctime = inode->change_time;
        
        ret->size = fs->get_inode_size(inode);

        ret->fs_data = entry->fs_data;
        ret->fs_id = entry->fs_id;
        ret->inode = entry->inode;
        ret->kflag_bitfield = 0;

        delete inode;
        return ret;
    }

    void ext2_t::ext2_convert_dentry(ext2_directory_entry_t *entry, dentry_t *vfs_entry){
        strncpy(vfs_entry->name, entry->name, min(entry->name_size_low, sizeof(vfs_entry->name)));

        vfs_entry->fs_data = this;
        vfs_entry->fs_id = this->filesystem_id;
        vfs_entry->inode = entry->inode;
        vfs_entry->__fs_fetch_vnode = ext2_fs_fetch_vnode;
    }

    int ext2_read(vnode_t *node, void *buffer, size_t size, size_t offset){
        filesystems::ext2_t* fs = (filesystems::ext2_t*)node->fs_data;

        return fs->load_inode_contents(node->inode, offset, size, buffer);
    }

    int ext2_write(vnode_t *node, const void *buffer, size_t size, size_t offset){
        filesystems::ext2_t* fs = (filesystems::ext2_t*)node->fs_data;
        node->attributes.mtime = current_time;

        int ret = fs->write_inode_contents(node->inode, offset, size, buffer);

        ext2_inode_t *inode = fs->fetch_inode(node->inode);
        node->size = fs->get_inode_size(inode);
        delete inode;

        return ret;
    }

    int ext2_truncate(vnode_t *node, size_t size){
        filesystems::ext2_t* fs = (filesystems::ext2_t*)node->fs_data;
        node->attributes.mtime = current_time;

        return fs->truncate_inode(node->inode, size);
    }

    int ext2_poll(vnode_t *node, int events, poll_table_t *pt){
        return POLLIN | POLLOUT;
    }

    int ext2_set_attributes(vnode_t *node, vnode_attributes_t *attrs){
        filesystems::ext2_t* fs = (filesystems::ext2_t*)node->fs_data;

        ext2_inode_t *inode = fs->fetch_inode(node->inode);
        
        if (attrs->valid & VNODE_ATTR_MODE) {
            inode->mode = attrs->mode & (~S_IFMT);
        }

        if (attrs->valid & VNODE_ATTR_UID) {
            inode->owner_uid = attrs->uid;
        }

        if (attrs->valid & VNODE_ATTR_GID) {
            inode->owner_gid = attrs->gid;
        }

        if (attrs->valid & VNODE_ATTR_ATIME) {
            inode->last_access_time = attrs->atime;
        }
        if (attrs->valid & VNODE_ATTR_MTIME) {
            inode->last_modification_time = attrs->mtime;
        }
        if (attrs->valid & VNODE_ATTR_CTIME) {
            inode->change_time = attrs->ctime;
        }
        
        fs->update_inode(node->inode, inode);
        delete inode;

        return 0;
    }

    dentry_t *ext2_lookup(vnode_t *node, const char *name) {
        filesystems::ext2_t* fs = (filesystems::ext2_t*)node->fs_data;

        uint64_t chunk_size = 4096;
        uint8_t* buffer = (uint8_t*)malloc(chunk_size);
        uint64_t file_offset = 0;

        while (true) {
            // Read a chunk of the directory file
            int64_t bytes_read = fs->load_inode_contents(node->inode, file_offset, chunk_size, buffer);
            
            // Stop if EOF (read 0 bytes) or Error (< 0)
            if (bytes_read <= 0) break;

            uint64_t buffer_pos = 0;
            
            while (buffer_pos < bytes_read) {
                ext2_directory_entry_t* entry = (ext2_directory_entry_t*)(buffer + buffer_pos);

                // Corrupted filesystem loop prevention
                if (entry->entry_size == 0) break; 

                // Process valid entries
                if (entry->inode != 0) {
                    if (strlen(name) != entry->name_size_low || strncmp(entry->name, name, entry->name_size_low)) {
                        buffer_pos += entry->entry_size;
                        continue;
                    }

                    ext2_inode_t* inode = fs->fetch_inode(entry->inode);

                    if (inode) {
                        dentry_t *ret = new dentry_t();
                        fs->ext2_convert_dentry(entry, ret);
                        
                        delete inode;
                        
                        // Cleanup and exit BOTH loops immediately
                        free(buffer);
                        return ret;
                    }
                }

                // Advance position inside the buffer
                buffer_pos += entry->entry_size;
            }

            // Advance the global file offset by what we actually read
            file_offset += bytes_read;
        }

        free(buffer);
        return nullptr;
    }

    int ext2_get_listing(vnode_t *node, dentry_t *&out, size_t offset, size_t limit){
        filesystems::ext2_t* fs = (filesystems::ext2_t*)node->fs_data;
        
        kstd::linked_list_t<dentry_t *> *temp_dir_list = new kstd::linked_list_t<dentry_t *>();

        uint64_t chunk_size = 4096;
        uint8_t* buffer = (uint8_t*)malloc(chunk_size);
        uint64_t file_offset = 0;

        int entries_parsed = 0;
        bool limit_reached = false;

        while (!limit_reached) {
            int64_t bytes_read = fs->load_inode_contents(node->inode, file_offset, chunk_size, buffer);
            if (bytes_read <= 0) break;

            uint64_t buffer_pos = 0;
            
            while (buffer_pos < bytes_read) {
                ext2_directory_entry_t* entry = (ext2_directory_entry_t*)(buffer + buffer_pos);

                if (entry->entry_size == 0) {
                    limit_reached = true;
                    break; 
                }

                if (entry->inode != 0) {
                    if (entries_parsed < offset) {
                        entries_parsed++;
                        buffer_pos += entry->entry_size;
                        continue;
                    }

                    dentry_t *new_dentry = new dentry_t();
                    fs->ext2_convert_dentry(entry, new_dentry);

                    temp_dir_list->add(new_dentry);
                    entries_parsed++;

                    if (temp_dir_list->size() >= limit) {
                        limit_reached = true;
                        break;
                    }
                }
                buffer_pos += entry->entry_size;
            }
            file_offset += bytes_read;
        }

        int count = temp_dir_list->size();

        if (count == 0){
            delete temp_dir_list;
            free(buffer);
            out = nullptr;
            return 0;
        }

        out = new dentry_t[count];
        for (int i = 0; i < count; i++){
            out[i] = *temp_dir_list->get(i);
            delete temp_dir_list->get(i);
        }

        delete temp_dir_list;
        free(buffer);

        return count;
    }

    int ext2_creat(vnode_t *node, const char *name, uint16_t mode){
        filesystems::ext2_t* fs = (filesystems::ext2_t*)node->fs_data;

        int ret = fs->create_file(node->inode, name, mode);

        ext2_inode_t* self = fs->fetch_inode(node->inode);
        node->size = fs->get_inode_size(self);

        delete self;
        return ret;
    }

    int ext2_mkdir(vnode_t *node, const char *name, uint16_t mode){
        filesystems::ext2_t* fs = (filesystems::ext2_t*)node->fs_data;

        int ret = fs->create_directory(node->inode, name, mode);

        ext2_inode_t* self = fs->fetch_inode(node->inode);
        node->size = fs->get_inode_size(self);

        delete self;
        return ret;
    }

    int ext2_unlink(vnode_t *node, const char *child){
        filesystems::ext2_t* fs = (filesystems::ext2_t*)node->fs_data;

        // Lookup the child
        dentry_t *dchild = ext2_lookup(node, child);
        if (!dchild) return -ENOENT;
        int cinode = dchild->inode;
        delete dchild;

        ext2_inode_t *ichild = fs->fetch_inode(cinode);

        ichild->hard_link_count--;

        fs->update_inode(cinode, ichild);
        
        fs->directory_remove_entry(node->inode, child);

        return 0;
    }

    int ext2_rmdir(vnode_t *node, const char *child) {
        filesystems::ext2_t* fs = (filesystems::ext2_t*)node->fs_data;

        dentry_t *dchild = ext2_lookup(node, child);
        if (!dchild) return -ENOENT;
        
        uint32_t child_inode_num = dchild->inode;
        delete dchild;

        if (child_inode_num == 2) return -EFAULT; // Protect Root

        // Fetch the child inode
        ext2_inode_t *child_inode = fs->fetch_inode(child_inode_num);
        if (!child_inode) return -EIO;

        if (!S_ISDIR(child_inode->mode)) {
            delete child_inode;
            return -ENOTDIR;
        }

        // Check for Empty Directory
        bool found_other_files = false;
        uint64_t chunk_size = 4096;
        uint8_t* buffer = (uint8_t*)malloc(chunk_size);
        uint64_t file_offset = 0;

        while (true) {
            int64_t bytes_read = fs->load_inode_contents(child_inode_num, file_offset, chunk_size, buffer);
            if (bytes_read <= 0) break;

            uint64_t buffer_pos = 0;
            while (buffer_pos < bytes_read) {
                ext2_directory_entry_t* entry = (ext2_directory_entry_t*)(buffer + buffer_pos);
                
                // Corrupted filesystem loop prevention
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
        
        if (found_other_files) {
            delete child_inode;
            return -ENOTEMPTY;
        }

        // Remove the entry from the parent directory
        fs->directory_remove_entry(node->inode, child);
        
        // Decrement parent link count (Removing the ".." reference)
        ext2_inode_t* parent_inode = fs->fetch_inode(node->inode);
        if (parent_inode) {
            if (parent_inode->hard_link_count > 0) {
                parent_inode->hard_link_count--;
            }
            fs->update_inode(node->inode, parent_inode);
            delete parent_inode;
        }

        // Set the child's link count to 0 (marks it for deletion)
        // The blocks are NOT freed here. The VFS will call evict_inode when ref_count hits 0.
        child_inode->hard_link_count = 0;
        fs->update_inode(child_inode_num, child_inode);
        delete child_inode;

        return 0;
    }

    void ext2_evict_inode(vnode_t *node){
        filesystems::ext2_t* fs = (filesystems::ext2_t*)node->fs_data;

        fs->free_inode_contents(node->inode);
    }

    vnode_file_operations_t ext2_file_operations = {
        .read = ext2_read,
        .write = ext2_write,
        .truncate = ext2_truncate,
        .poll = ext2_poll,
        .set_attributes = ext2_set_attributes,
        .lookup = ext2_lookup,
        .get_listing = ext2_get_listing,
        .creat = ext2_creat,
        .mkdir = ext2_mkdir,
        .unlink = ext2_unlink,
        .rmdir = ext2_rmdir,
        .evict_inode = ext2_evict_inode,
    };
}
