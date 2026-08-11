#include <vfs/vfs.h>
#include <vfs/vnode.h>
#include <vfs/dentry.h>
#include <structures/hashmap/hashmap.h>
#include <vfs/vnode_cache.h>
#include <kerrno.h>
#include <filepath.h>
#include <scheduling/task_scheduler/task_scheduler.h>

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
        target->parent = mountpoint;

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

    int vfs_check_permission(vnode_t *node, task_t *task, int want_mask) {
        if (!node || !task) return -EACCES;

        uint16_t mode = node->attributes.mode;
        uint32_t file_uid = node->attributes.uid;
        uint32_t file_gid = node->attributes.gid;

        
        uint32_t task_uid = task->euid; 
        uint32_t task_gid = task->egid;

        // If the 31st bit is set, use ruid/rgid
        if (want_mask & (1ULL << 31)){
            want_mask &= ~(1ULL << 31);
            task_uid = task->ruid;
            task_gid = task->rgid;
        }

        // Superuser (Root) Bypass
        if (task_uid == 0) {
            // Root can always read and write.
            // For execution, root can only execute if the file is a directory, 
            // OR if at least one execute bit (User, Group, or Other) is set.
            if (want_mask & MAY_EXEC) {
                bool is_dir = ((mode & S_IFMT) == S_IFDIR);
                bool any_exec_bit = (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
                
                if (!is_dir && !any_exec_bit) {
                    return -EACCES;
                }
            }
            return 0;
        }

        // Determine which permission block applies
        int granted_perms = 0;

        if (task_uid == file_uid) {
            // We own the file: shift mode right by 6 bits to get owner rwx
            granted_perms = (mode >> 6) & 7; 
        } 
        else if (task_gid == file_gid) {
            // We are in the file's group: shift right by 3 bits to get group rwx
            // Note to future me: When supplimentarry groups are added, they should be checked here
            granted_perms = (mode >> 3) & 7;
        } 
        else {
            // We are "Others": the bottom 3 bits are the world
            granted_perms = mode & 7;
        }

        // Compare what we WANT against what is GRANTED
        if ((granted_perms & want_mask) == want_mask) {
            return 0; // Success!
        }

        return -EACCES;
    }

    dentry_t *_resolve_path_internal(dentry_t *base_dir, const char *path, bool follow_symlinks, bool follow_trailing, int depth, int &err) {
        if (!path || path[0] == '\0') {
            err = -ENOENT;
            return nullptr;
        }
        if (depth > MAX_SYMLINK_DEPTH) {
            err = -ELOOP;
            return nullptr; 
        }

        dentry_t *current = base_dir;
        if (path[0] == '/') current = root_entry;
        
        if (!current) {
            err = -ENOENT;
            return nullptr;
        }
        
        current->ref();

        while (current && current->mounted_root) {
            dentry_t *mount = current->mounted_root;
            mount->ref();
            current->unref();
            current = mount;
        }

        size_t len = strlen(path);
        char *path_clone = new char[len + 1];
        strcpy(path_clone, path);

        char *rest = path_clone;
        char *token = strtok_r(path_clone, "/", &rest);

        // Grab the current task for permission checking. 
        task_t *task = task_scheduler::get_current_task();

        while (token != nullptr) {
            char *next_token = strtok_r(nullptr, "/", &rest);
            bool is_last = (next_token == nullptr);

            // Handle .
            if (strcmp(token, ".") == 0) {
                token = next_token;
                continue;
            }

            // Handle ..
            if (strcmp(token, "..") == 0) {
                if (current != root_entry && current->parent != nullptr) {
                    dentry_t *parent = current->parent;

                    // If 'parent' is a covered mountpoint, bypass it and keep stepping up
                    while (parent && parent->mounted_root && parent->parent != nullptr) {
                        parent = parent->parent;
                    }

                    parent->ref();
                    current->unref();
                    current = parent;
                }
                token = next_token;
                continue;
            }

            // To traverse a directory, the user MUST have MAY_EXEC on it.
            // We skip this if task is nullptr (kernel internal operation).
            if (task != nullptr && task->is_userspace) {
                vnode_t *dir_vnode = _get_vnode(current);
                if (dir_vnode) {
                    int perm = vfs_check_permission(dir_vnode, task, MAY_EXEC) < 0;
                    dir_vnode->close();
                    
                    if (perm < 0) {
                        err = -EACCES;
                        current->unref();
                        delete[] path_clone;
                        return nullptr;
                    }
                }
            }

            dentry_t *next = _get_dentry(current, token);
            dentry_t *parent_dir = current; 
            
            if (!next) {
                err = -ENOENT;
                parent_dir->unref();
                delete[] path_clone;
                return nullptr;
            }

            while (next && next->mounted_root) {
                dentry_t *mount = next->mounted_root;
                mount->ref();
                next->unref();
                next = mount;
            }

            vnode_t *vnode = _get_vnode(next);
            bool is_symlink = vnode && ((vnode->attributes.mode & S_IFMT) == S_IFLNK);

            if (vnode) {
                bool is_symlink = ((vnode->attributes.mode & S_IFMT) == S_IFLNK);
                bool should_follow = is_symlink && (is_last ? follow_trailing : follow_symlinks);

                if (should_follow) {
                    char link_target[1024] = {0}; 
                    int read_bytes = vnode->read(link_target, sizeof(link_target) - 1, 0);

                    vnode->close();

                    if (read_bytes < 0) {
                        err = read_bytes; // Pass up the underlying read error
                        next->unref();
                        parent_dir->unref();
                        delete[] path_clone;
                        return nullptr;
                    }
                    
                    dentry_t *link_base = (link_target[0] == '/') ? root_entry : parent_dir;
                    bool follow_link_trailing = is_last ? follow_trailing : true;

                    // Recursive call passes the err reference down!
                    dentry_t *resolved_link = _resolve_path_internal(link_base, link_target, follow_symlinks, follow_link_trailing, depth + 1, err);
                    
                    next->unref(); 
                    next = resolved_link; 
                    
                    if (!next) {
                        // err is already set by the recursive call
                        parent_dir->unref();
                        delete[] path_clone;
                        return nullptr;
                    }
                } else {
                    vnode->close(); // Probably regular file
                }
            }

            parent_dir->unref(); 
            current = next;
            token = next_token;
        }

        delete[] path_clone;
        err = 0; // Success!
        return current;
    }
    

    vnode_t *resolve_path(const char *path, int intent, int &err, bool follow_symlinks, bool follow_trailing) {
        task_t *self = task_scheduler::get_current_task();
        dentry_t *base = root_entry;

        if (self && self->fd_table && self->fd_table->cwd){
            base = self->fd_table->cwd;
        }
        

        dentry_t *dentry = _resolve_path_internal(base, path, follow_symlinks, follow_trailing, 0, err);
        if (!dentry) return nullptr;

        vnode_t *vnode = _get_vnode(dentry);
        
        task_t *task = task_scheduler::get_current_task();
        if (task != nullptr && intent != 0 && vnode != nullptr) {
            if (vfs_check_permission(vnode, task, intent) < 0) {
                err = -EACCES;
                dentry->unref();
                vnode->close();
                return nullptr;
            }
        }

        dentry->unref(); 
        return vnode;
    }

    dentry_t *resolve_path_dentry(const char *path, int intent, int &err, bool follow_symlinks, bool follow_trailing) {
        task_t *self = task_scheduler::get_current_task();
        dentry_t *base = root_entry;

        if (self && self->fd_table && self->fd_table->cwd){
            base = self->fd_table->cwd;
        }

        dentry_t *dentry = _resolve_path_internal(base, path, follow_symlinks, follow_trailing, 0, err);
        if (!dentry) return nullptr;

        task_t *task = task_scheduler::get_current_task();
        if (task != nullptr && intent != 0) {
            vnode_t *vnode = _get_vnode(dentry);
            if (vnode) {
                int perm = vfs_check_permission(vnode, task, intent);
                vnode->close();
                
                if (perm < 0) {
                    err = -EACCES;
                    dentry->unref();
                    return nullptr;
                }
            }
        }

        return dentry;
    }

    dentry_t *resolve_path_dentry_at(dentry_t *base_dir, const char *path, int intent, int &err, bool follow_symlinks, bool follow_trailing) {
        if (!base_dir) base_dir = get_root();
        
        dentry_t *dentry = _resolve_path_internal(base_dir, path, follow_symlinks, follow_trailing, 0, err);
        if (!dentry) return nullptr;

        task_t *task = task_scheduler::get_current_task();
        if (task != nullptr && intent != 0) {
            vnode_t *vnode = _get_vnode(dentry);
            if (vnode) {
                int perm = vfs_check_permission(vnode, task, intent);
                vnode->close();
                
                if (perm < 0) {
                    err = -EACCES;
                    dentry->unref();
                    return nullptr;
                }
            }
        }

        return dentry;
    }

    vnode_t *resolve_path(const char *path){
        int err;
        return resolve_path(path, MAY_READ | MAY_WRITE | MAY_EXEC, err);
    }

    dentry_t *resolve_path_dentry(const char *path){
        int err;
        return resolve_path_dentry(path, MAY_READ | MAY_WRITE | MAY_EXEC, err);
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

    int mklink(const char *path, const char *target) {
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
            status = parent_vnode->mklink(name, target);
        }

        // Cleanup reference counts
        parent_vnode->close();
        parent_dentry->unref();

        return status;
    }
}