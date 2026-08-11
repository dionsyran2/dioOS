#include <drivers/filesystems/ramfs/ramfs.h>
#include <drivers/filesystems/ramfs/node.h>
#include <drivers/filesystems/ramfs/dentry.h>
#include <drivers/timers/common.h>
#include <bits/poll.h>
#include <vfs/vfs.h>
#include <cstr.h>

namespace ramfs {
    extern vnode_file_operations_t operation_table;

    uint32_t convert_type_to_mode(rfs_type_t type){
        switch (type) {
            case DIR:
                return S_IFDIR;
            case REG:
                return S_IFREG;
            case LNK:
                return S_IFLNK;
        }

        return 0;
    }
    
    vnode_t *vfs_fetch_vnode(dentry_t *entry){
        __ramfs *fs = (__ramfs *)entry->fs_data;
        
        rfs_vnode_t *rfs_node = fs->get_node(entry->inode);

        if (!rfs_node) return nullptr;

        vnode_t *vnode = new vnode_t();
        vnode->fs_id = fs->fs_id;

        vnode->inode = entry->inode;
        vnode->operations = &operation_table;
        vnode->size = rfs_node->file_size;
        vnode->fs_data = fs;
        vnode->nlink = rfs_node->nlink; // hardcoded to 1 static link for now

        vnode->attributes = rfs_node->attributes;
        vnode->attributes.mode = convert_type_to_mode(rfs_node->type) | (rfs_node->attributes.mode & 0777);
        
        return vnode;
    }

    void convert_to_vfs_dentry(rfs_dentry_t *src, dentry_t *dst, __ramfs *fs){
        strcpy(dst->name, src->name);
        dst->inode = src->inode;
        dst->fs_id = fs->fs_id;
        dst->fs_data = fs;
        dst->__fs_fetch_vnode = vfs_fetch_vnode;
    }

    int vfs_read(vnode_t *node, void *buffer, size_t size, size_t offset){
        __ramfs *fs = (__ramfs *)node->fs_data;
        rfs_vnode_t *rnode = fs->get_node(node->inode);

        node->attributes.atime = rnode->attributes.atime = current_time; // Update the last access time

        return rnode->read(buffer, size, offset);
    }

    int vfs_write(vnode_t *node, const void *buffer, size_t size, size_t offset){
        __ramfs *fs = (__ramfs *)node->fs_data;
        rfs_vnode_t *rnode = fs->get_node(node->inode);
        node->size = rnode->file_size;

        int ret = rnode->write(buffer, size, offset);

        node->size = rnode->file_size;
        node->attributes.mtime = rnode->attributes.mtime = current_time; // Update the last modification time
        node->attributes.ctime = rnode->attributes.ctime = current_time; // Note: ctime always updates when mtime updates
        return ret;
    }

    dentry_t *vfs_lookup(vnode_t *node, const char *name){
        __ramfs *fs = (__ramfs *)node->fs_data;
        rfs_vnode_t *rnode = fs->get_node(node->inode);

        rfs_dentry_t *rentry = rnode->lookup(name);

        if (!rentry) return nullptr;

        dentry_t *dentry = new dentry_t("", 0, 0);
        convert_to_vfs_dentry(rentry, dentry, fs);

        return dentry;
    }

    int vfs_get_listing(vnode_t *node, dentry_t *&out, size_t offset, size_t limit){
        __ramfs *fs = (__ramfs *)node->fs_data;
        rfs_vnode_t *rnode = fs->get_node(node->inode);

        rnode->children.lock();
        int size = rnode->children.size();
        
        if (size < offset){
            rnode->children.unlock();
            return 0;
        }
        
        dentry_t *array = new dentry_t[min(size - offset, limit)];

        int count = 0;
        for (int i = offset; i < size; i++){
            if (count >= limit) break;

            rfs_dentry_t *rfs_dentry = rnode->children.get(i);
            convert_to_vfs_dentry(rfs_dentry, &array[i], fs);
            count++;
        }

        rnode->children.unlock();
        out = array;
        return size;
    }

    int vfs_creat(vnode_t *node, const char *name, uint16_t mode){
        __ramfs *fs = (__ramfs *)node->fs_data;
        rfs_vnode_t *rnode = fs->get_node(node->inode);

        int inode = fs->allocate_vnode(REG, mode);
        rnode->creat(name, inode);

        return 0;
    }
    int vfs_mkdir(vnode_t *node, const char *name, uint16_t mode){
        __ramfs *fs = (__ramfs *)node->fs_data;
        rfs_vnode_t *rnode = fs->get_node(node->inode);

        int inode = fs->allocate_vnode(DIR, mode);
        rnode->mkdir(name, inode);

        return 0;
    }

    int vfs_mklink(vnode_t *node, const char *name, const char *target){
        __ramfs *fs = (__ramfs *)node->fs_data;
        rfs_vnode_t *rnode = fs->get_node(node->inode);

        int inode = fs->allocate_vnode(LNK, 0777);
        rnode->mklink(name, inode);

        rfs_vnode_t *lnk = fs->get_node(inode);
        lnk->write(target, strlen(target), 0);

        return 0;
    }

    int vfs_unlink(vnode_t *node, const char *child){
        __ramfs *fs = (__ramfs *)node->fs_data;
        rfs_vnode_t *rnode = fs->get_node(node->inode);

        int r = rnode->unlink(child);

        return r;
    }

    void vfs_evict_inode(vnode_t *node){
        __ramfs *fs = (__ramfs *)node->fs_data;

        // Well... this is the end i guess.
        fs->free_vnode(node->inode);
    }

    int vfs_poll(vnode_t *node, int events, poll_table_t *pt){
        return (POLLIN | POLLOUT) & events; // It will always return POLLIN | POLLOUT
    }

    int vfs_set_attributes(vnode_t *node, vnode_attributes_t *attrs){
        __ramfs *fs = (__ramfs *)node->fs_data;
        rfs_vnode_t *rnode = fs->get_node(node->inode);

        // Collect all of the valid attributes in 'attrs' and make a valid block of them
        vnode_attributes_t new_attrs = node->attributes;

        if (attrs->valid & VNODE_ATTR_MODE){
            new_attrs.mode = (new_attrs.mode & S_IFMT) | (attrs->mode & 0777);
        }

        if (attrs->valid & VNODE_ATTR_UID){
            new_attrs.uid = attrs->uid;
        }

        if (attrs->valid & VNODE_ATTR_GID){
            new_attrs.gid = attrs->gid;
        }

        if (attrs->valid & VNODE_ATTR_ATIME){
            new_attrs.atime = attrs->atime;
        }

        if (attrs->valid & VNODE_ATTR_MTIME){
            new_attrs.mtime = attrs->mtime;
        }

        if (attrs->valid & VNODE_ATTR_CTIME){
            new_attrs.ctime = attrs->ctime;
        }

        if (attrs->valid & VNODE_ATTR_BTIME){
            new_attrs.btime = attrs->btime;
        }

        // Update the attributes
        rnode->attributes = new_attrs;
        node->attributes = new_attrs;
        // Update the metadata change time
        rnode->attributes.ctime = current_time;
        return 0;
    }

    vnode_file_operations_t operation_table = {
        .read = vfs_read,
        .write = vfs_write,
        .set_attributes = vfs_set_attributes,
        .lookup = vfs_lookup,
        .get_listing = vfs_get_listing,
        .creat = vfs_creat,
        .mkdir = vfs_mkdir,
        .mklink = vfs_mklink,
        .unlink = vfs_unlink,
        .evict_inode = vfs_evict_inode,
    };

    int __ramfs::allocate_vnode(rfs_type_t type, uint16_t mode){
        rfs_vnode_t *vnode = new rfs_vnode_t();
        vnode->type = type;
        vnode->nlink = 1; // Currently hardcoded
        vnode->attributes.ctime = vnode->attributes.atime = vnode->attributes.btime = vnode->attributes.mtime = current_time; // Set the time values
        vnode->attributes.mode = mode; // Set the permissions
        vnode->attributes.valid = ( VNODE_ATTR_MODE | VNODE_ATTR_UID | VNODE_ATTR_GID | VNODE_ATTR_ATIME
                                    | VNODE_ATTR_MTIME | VNODE_ATTR_CTIME | VNODE_ATTR_BTIME ); // Set which fields are valid to be updated

        int id = __atomic_fetch_add(&this->current_inode_id, 1, __ATOMIC_SEQ_CST);

        this->vnode_tree.insert(id, vnode);
        return id;
    }

    void __ramfs::free_vnode(int inode){
        rfs_vnode_t *node = this->vnode_tree.search(inode);

        if (!node) return;

        this->vnode_tree.remove(inode);
        delete node;
    }

    rfs_vnode_t *__ramfs::get_node(int identifier){
        return this->vnode_tree.search(identifier);
    }

    dentry_t *create_fs(){
        __ramfs *ramfs = new __ramfs();
        int fs_id = vfs::allocate_filesystem_id();
        ramfs->fs_id = fs_id;

        dentry_t *dentry = new dentry_t("root", ramfs->allocate_vnode(rfs_type_t::DIR, 0777), fs_id);
        dentry->__fs_fetch_vnode = vfs_fetch_vnode;
        dentry->fs_data = ramfs;
        return dentry;
    }
}