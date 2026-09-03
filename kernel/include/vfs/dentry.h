#pragma once
#include <stdint.h>
#include <stddef.h>
#include <structures/lists/linked_list.h>

struct vnode_t;

struct dentry_t{
    char name[256];
    
    dentry_t *parent;

    kstd::linked_list_t<dentry_t*> children;

    // Mounting point redirection
    // If a filesystem is mounted here, this redirects to the mounted filesystem's root dentry.
    dentry_t* mounted_root;

    uint32_t ref_count;

    uint32_t kflags;

    
    // fs data
    void *fs_data;
    uint32_t inode;
    uint32_t fs_id;

    dentry_t();
    dentry_t(const char* name, uint32_t inode, uint32_t fs_id);
    ~dentry_t();

    void ref();
    void unref();

    // Traversal & Cache Utilities
    dentry_t* lookup_child(const char* name);
    void add_child(dentry_t* child);

    // Fetch the vnode this dentry is pointing to
    vnode_t *fetch_vnode();

    vnode_t *(*__fs_fetch_vnode)(dentry_t *entry) = nullptr;
};