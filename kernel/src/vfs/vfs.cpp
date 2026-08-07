#include <vfs/vfs.h>
#include <vfs/vnode.h>
#include <vfs/dentry.h>
#include <structures/hashmap/hashmap.h>
#include <vfs/vnode_cache.h>
#include <kerrno.h>

namespace vfs{
    dentry_t *root_entry;
    vnode_cache_t *vnode_cache;

    int current_filesystem_id = 0;

    void initialize(){
        // The root is purely a virtual structure... its only purpose is to serve as a point to mount a filesystem.
        root_entry = new dentry_t("/", 0 /* No inode */, 0 /* Not a part of a filesystem */);
        root_entry->ref();

        vnode_cache = new vnode_cache_t();
    }
    
    void __release_vnode(vnode_t *node){
        vnode_cache->release(node);
    }

    void __invalidate_vnode_cache(vnode_t *node) {
        if (!node || !vnode_cache) return;
        vnode_cache->invalidate(node);
    }

    dentry_t *resolve_path_dentry(const char *path);
    void __invalidate_dentry_cache(const char *path) {
        // Resolve the target dentry from the path
        dentry_t *target = resolve_path_dentry(path);
        if (!target) return;

        dentry_t *parent = target->parent;
        if (parent) {
            // Remove target dentry from parent's children list
            parent->children.lock();
            for (int i = 0; i < parent->children.size(); i++) {
                if (parent->children.get(i) == target) {
                    parent->children.remove(i);
                    target->parent = nullptr;
                    target->unref(); // Drop parent's reference
                    break;
                }
            }
            parent->children.unlock();
        }

        // Release the reference acquired by resolve_path_dentry
        target->unref();
    }

    int allocate_filesystem_id(){
        return __atomic_fetch_add(&current_filesystem_id, 1, __ATOMIC_SEQ_CST);
    }

    void mount(dentry_t *mountpoint, dentry_t *target){
        mountpoint->mounted_root = target;
        mountpoint->ref();
        target->ref();
    }

    int unmount(dentry_t *mountpoint){
        if (!mountpoint || !mountpoint->mounted_root) return -ENOENT;
        if (mountpoint->mounted_root->ref_count > 1) return -EBUSY;
        
        mountpoint->mounted_root->unref();
        mountpoint->mounted_root = nullptr;
        mountpoint->unref();

        return 0;
    }

    dentry_t *get_root(){
        root_entry->ref();
        return root_entry;
    }

    vnode_t *_get_vnode(dentry_t *dentry) {
        if (!dentry) return nullptr;

        vnode_t *ret = vnode_cache->fetch(dentry->fs_id, dentry->inode);

        if (!ret) {
            ret = dentry->fetch_vnode();
            if (!ret) return nullptr;

            vnode_cache->add(ret);
        }

        ret->open();
        return ret;
    }

    dentry_t *_get_dentry(dentry_t *parent, const char *name) {
        if (!parent || !name) return nullptr;

        dentry_t *ret = parent->lookup_child(name);

        if (!ret) {
            vnode_t *vnode = _get_vnode(parent);
            if (!vnode) return nullptr;

            ret = vnode->lookup(name);
            vnode->close();

            if (!ret) return nullptr;

            parent->add_child(ret);
            ret->ref();
        }

        return ret;
    }

    vnode_t *resolve_path(const char *path) {
        if (!path || path[0] == '\0') return nullptr;

        // Clone path string to heap buffer for strtok_r mutation
        size_t len = strlen(path);
        char *path_clone = new char[len + 1];
        strcpy(path_clone, path);

        char *rest = path_clone;
        char *token = strtok_r(path_clone, "/", &rest);

        dentry_t *current = root_entry;
        if (!current) {
            delete[] path_clone;
            return nullptr;
        }

        current->ref();

        while (current && current->mounted_root) {
            dentry_t *mount = current->mounted_root;
            mount->ref();
            current->unref();
            current = mount;
        }

        while (token != nullptr) {
            if (!current) break;

            dentry_t *next = _get_dentry(current, token);
            current->unref();
            current = next;

            if (!current) break;

            while (current && current->mounted_root) {
                dentry_t *mount = current->mounted_root;
                mount->ref();
                current->unref();
                current = mount;
            }

            token = strtok_r(nullptr, "/", &rest);
        }

        vnode_t *resolved_vnode = nullptr;
        if (current) {
            resolved_vnode = _get_vnode(current);
            current->unref();
        }

        delete[] path_clone;

        return resolved_vnode;
    }


    dentry_t *resolve_path_dentry(const char *path) {
        if (!path || path[0] == '\0') return nullptr;

        size_t len = strlen(path);
        char *path_clone = new char[len + 1];
        strcpy(path_clone, path);

        char *rest = path_clone;
        char *token = strtok_r(path_clone, "/", &rest);

        dentry_t *current = root_entry;
        if (!current) {
            delete[] path_clone;
            return nullptr;
        }

        current->ref();

        while (current && current->mounted_root) {
            dentry_t *mount = current->mounted_root;
            mount->ref();
            current->unref();
            current = mount;
        }

        while (token != nullptr) {
            if (!current) break;

            dentry_t *next = _get_dentry(current, token);
            current->unref();
            current = next;

            if (!current) break;

            while (current && current->mounted_root) {
                dentry_t *mount = current->mounted_root;
                mount->ref();
                current->unref();
                current = mount;
            }

            token = strtok_r(nullptr, "/", &rest);
        }

        delete[] path_clone;

        return current;
    }

    static bool split_path(const char *path, char *parent_path, char *target_name) {
        if (!path || path[0] == '\0') return false;

        // Find the last slash
        const char *last_slash = strrchr(path, '/');

        if (!last_slash) {
            // Relative path without slashes (e.g., "foo")
            strcpy(parent_path, ".");
            strcpy(target_name, path);
        } else if (last_slash == path) {
            // Target lives in root directory (e.g., "/foo")
            strcpy(parent_path, "/");
            strcpy(target_name, last_slash + 1);
        } else {
            // Nested path (e.g., "/a/b/c")
            size_t parent_len = last_slash - path;
            strncpy(parent_path, path, parent_len);
            parent_path[parent_len] = '\0';
            strcpy(target_name, last_slash + 1);
        }

        // Edge case: path ended with a trailing slash (e.g., "/a/b/")
        if (target_name[0] == '\0') return false;

        return true;
    }


    int mkdir(const char *path, uint16_t mode) {
        char parent_path[512];
        char name[128];

        if (!split_path(path, parent_path, name)) {
            return -1; // Invalid path format
        }

        dentry_t *parent_dentry = resolve_path_dentry(parent_path);
        if (!parent_dentry) return -1; // Parent directory does not exist

        vnode_t *parent_vnode = _get_vnode(parent_dentry);
        if (!parent_vnode) {
            parent_dentry->unref();
            return -1;
        }

        int status = -1;
        if (parent_vnode->operations && parent_vnode->operations->mkdir) {
            status = parent_vnode->mkdir(name, mode);
        }

        // Cleanup reference counts
        parent_vnode->close();
        parent_dentry->unref();

        return status;
    }

    int mkfile(const char *path, uint16_t mode) {
        char parent_path[512];
        char name[128];

        if (!split_path(path, parent_path, name)) {
            return -1; // Invalid path format
        }

        dentry_t *parent_dentry = resolve_path_dentry(parent_path);
        if (!parent_dentry) return -1; // Parent directory does not exist

        vnode_t *parent_vnode = _get_vnode(parent_dentry);
        if (!parent_vnode) {
            parent_dentry->unref();
            return -1;
        }

        int status = -1;
        if (parent_vnode->operations && parent_vnode->operations->creat) {
            status = parent_vnode->creat(name, mode);
        }

        // Cleanup reference counts
        parent_vnode->close();
        parent_dentry->unref();

        return status;
    }
}