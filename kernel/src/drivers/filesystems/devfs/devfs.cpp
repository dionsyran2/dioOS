#include <drivers/filesystems/devfs/devfs.h>
#include <drivers/filesystems/devfs/devfs_entry.h>
#include <cstr.h>
#include <filepath.h>
#include <kerrno.h>
#include <math.h>
#include <drivers/timers/common.h>
namespace devfs{
    devfs_entry_t *root = nullptr;
    int devfs_id = 0;
    int devfs_inode = 0;
    
    // Forward declarations
    dentry_t *create_dentry(devfs_entry_t *entry);
    vnode_t *create_vnode(devfs_entry_t *entry);

    // Functions
    int vfs_read(vnode_t *node, void *buffer, size_t size, size_t offset){
        // Find the entry
        devfs_entry_t *entry = (devfs_entry_t *)node->fs_data;

        // Call the read function
        return entry->read(buffer, size, offset);
    }

    int vfs_write(vnode_t *node, const void *buffer, size_t size, size_t offset){
        // Find the entry
        devfs_entry_t *entry = (devfs_entry_t *)node->fs_data;

        // Call the write function
        return entry->write(buffer, size, offset);
    }

    dentry_t *vfs_lookup(vnode_t *node, const char *name){
        // Find the entry
        devfs_entry_t *entry = (devfs_entry_t *)node->fs_data;

        // Fail if the children list is not allocated
        if (!entry->children) return nullptr;

        // Acquire the lock
        entry->children->lock();

        // Try to find the child
        devfs_entry_t *child = nullptr;
        for (int i = 0; i < entry->children->size(); i++){
            // Get the current entry
            devfs_entry_t *current = entry->children->get(i);

            if (strcmp(current->name, name)) continue; // Skip if not a match

            // Save and break
            child = current;
            break;
        }

        // Free the lock
        entry->children->unlock();

        // If there no child, return
        if (!child) return nullptr;

        // Create a dentry based of that child
        dentry_t *ret = create_dentry(child);
        return ret;
    }

    int vfs_get_listing(vnode_t *node, dentry_t *&out, size_t offset, size_t limit){
        // Find the entry
        devfs_entry_t *entry = (devfs_entry_t *)node->fs_data;

        // Fail if the children list is not allocated
        if (!entry->children) return 0;

        // Acquire the lock
        entry->children->lock();

        // Verify the parameters
        size_t rem = entry->children->size() - offset;
        rem = min(rem, limit); // Limit the rem value

        if (rem == 0){
            // Free the lock and return
            entry->children->unlock();
            return 0;
        }

        dentry_t *buffer = new dentry_t[rem];

        for (int i = offset; i < entry->children->size(); i++){
            // Get the current entry
            devfs_entry_t *current = entry->children->get(i);

            // Create a dentry for that node
            dentry_t *dentry = create_dentry(current);

            // Copy that to the allocated buffer
            memcpy(buffer + i, dentry, sizeof(dentry_t));

            // delete the dentry
            delete dentry;
        }

        entry->children->unlock();
        out = buffer;
        return rem;
    }

    
    int vfs_ioctl(vnode_t *node, int op, char* argp){
        // Find the entry
        devfs_entry_t *entry = (devfs_entry_t *)node->fs_data;

        return entry->ioctl(op, argp);
    }

    int vfs_poll(vnode_t *node, int events, poll_table_t *pt){
        devfs_entry_t *entry = (devfs_entry_t *)node->fs_data;

        return entry->poll(events, pt);
    }

    vnode_t *vfs_fetch_vnode(dentry_t *dentry){
        // Find the entry
        devfs_entry_t *entry = (devfs_entry_t *)dentry->fs_data;
        
        return create_vnode(entry);
    }

    int vfs_set_attributes(vnode_t *node, vnode_attributes_t *attrs){
        devfs_entry_t *entry = (devfs_entry_t *)node->fs_data;

        // Collect all of the valid attributes in 'attrs' and make a valid block of them
        vnode_attributes_t new_attrs = node->attributes;

        if (attrs->valid & VNODE_ATTR_MODE){
            //new_attrs.mode = (new_attrs.mode & S_IFMT) | (attrs->mode & 0777);
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
        entry->attributes = new_attrs;

        // Update the metadata change time
        entry->attributes.ctime = current_time;
        node->attributes = new_attrs;

        return 0;
    }

    vnode_file_operations_t vfs_operations = {
        .read = vfs_read,
        .write = vfs_write,
        .ioctl = vfs_ioctl,
        .poll = vfs_poll,
        .set_attributes = vfs_set_attributes,
        .lookup = vfs_lookup,
        .get_listing = vfs_get_listing,
    };


    vnode_t *create_vnode(devfs_entry_t *entry){
        vnode_t *ret = new vnode_t();
        ret->operations = &vfs_operations;
        ret->nlink = 1;
        ret->fs_id = devfs_id; 
        ret->fs_data = entry;
        ret->inode = entry->inode;
        
        ret->attributes = entry->attributes; 

        return ret;
    }

    dentry_t *create_dentry(devfs_entry_t *entry){
        dentry_t *ret = new dentry_t(entry->name, entry->inode, devfs_id);
        ret->__fs_fetch_vnode = vfs_fetch_vnode;
        ret->fs_data = entry;

        return ret;
    }

    devfs_entry_t *__create_devfs_entry(devfs_entry_t *parent, char *name, uint16_t mode, devfs_ops_t *operations, void *ctx){
        devfs_entry_t *entry = new devfs_entry_t(name, mode, ctx, operations);
        entry->inode = __atomic_add_fetch(&devfs_inode, 1, __ATOMIC_SEQ_CST);

        if (parent && parent->children) {
            parent->children->lock();
            parent->children->add(entry);
            parent->children->unlock();
        }

        return entry;
    }

    devfs_entry_t *__internal_resolve_path(const char *path){
        if (!path || path[0] == '\0') return nullptr;

        size_t len = strlen(path);
        char *path_clone = new char[len + 1];
        strcpy(path_clone, path);

        char *rest = path_clone;
        char *token = strtok_r(path_clone, "/", &rest);

        devfs_entry_t *current = root;
        if (!current) {
            delete[] path_clone;
            return nullptr;
        }

        while (token != nullptr) {
            // FIX: If we can't go deeper, the path is invalid!
            if (!current || !current->children) {
                current = nullptr;
                break;
            }

            devfs_entry_t *next = nullptr;
            current->children->lock();
            for (int i = 0; i < current->children->size(); i++){
                devfs_entry_t *c = current->children->get(i);
                if (strcmp(c->name, token) == 0) {
                    next = c;
                    break;
                }
            }
            current->children->unlock();

            current = next;
            if (!current) break;

            token = strtok_r(nullptr, "/", &rest);
        }

        delete[] path_clone;
        return current;
    }

    void create_devfs_entry(devfs_entry_t *parent, char *name, uint16_t mode, devfs_ops_t *operations, vnode_t *ctx){
        devfs_entry_t *entry = new devfs_entry_t(name, mode, ctx, operations);
        entry->inode = __atomic_add_fetch(&devfs_inode, 1, __ATOMIC_SEQ_CST);

        if (parent && parent->children) {
            parent->children->lock();
            parent->children->add(entry);
            parent->children->unlock();
        }
    }

    void init(){
        devfs_id = vfs::allocate_filesystem_id();
        // S_IFDIR plus standard 0755 permissions
        root = __create_devfs_entry(nullptr, "/", S_IFDIR | 0755, nullptr, nullptr);
    }


    vnode_t *resolve_path(const char *path){
        if (!root) init();

        devfs_entry_t *e = __internal_resolve_path(path);

        if (!e) return nullptr;

        vnode_t *r = create_vnode(e);
        r->open();
        return r;
    }

    dentry_t *resolve_path_dentry(const char *path){
        if (!root) init();

        devfs_entry_t *e = __internal_resolve_path(path);

        if (!e) return nullptr;

        dentry_t *r = create_dentry(e);
        r->ref();
        return r;
    }

    int mknod(const char *path, uint16_t mode, devfs_ops_t *operations, void *ctx){
        if (!root) init();

        char parent_path[512];
        char name[128];

        if (!split_path(path, parent_path, name)) {
            return -1; // Invalid path format
        }

        devfs_entry_t *parent_entry = __internal_resolve_path(parent_path);
        if (!parent_entry) return -1; // Parent directory does not exist
        if (!parent_entry->children) return -ENOTDIR;
        
        parent_entry->children->lock();
        for (int i = 0; i < parent_entry->children->size(); i++){
            devfs_entry_t *entry = parent_entry->children->get(i);

            if (strcmp(name, entry->name)) continue;

            parent_entry->children->unlock();
            return -EEXIST;
        }

        parent_entry->children->unlock();

        __create_devfs_entry(parent_entry, name, mode, operations, ctx);

        return 0;
    }
}