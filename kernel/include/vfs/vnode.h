#pragma once
#include <stdint.h>
#include <stddef.h>
#include <structures/lists/linked_list.h>
#include <scheduling/mutex/mutex.h>
#include <vfs/vnode_attributes.h>
#include <vfs/vnode_operations.h>
#include <vfs/vnode_locks.h>
#include <vfs/vnode_flags.h>
struct dentry_t;


struct vnode_t {
    vnode_file_operations_t *operations;

    int nlink;

    vnode_attributes_t attributes;
    uint64_t size;

    uint32_t ref_count;

    // Filesystem Specific
    void *fs_data;
    
    uint32_t inode;
    uint32_t fs_id;

    // Syncronisation
    // Userspace (Read/Write locks via fcntl and flock)
    // fcntl can lock specific regions while flock locks the entire file (0 -> UINT64_MAX)
    kstd::linked_list_t<file_lock_t *> lock_list; // The task should also have a similar list
                                                  // So locks get released when the task exits
    
    // Kernel Locking
    mutex_t klock; // Acquire before read-write operations
    spinlock_t metadata_lock = 0;

    // Kernel Flags
    uint32_t kflag_bitfield = 0; // Used to track the current state (e.g vnode caching state etc.)

    // Operation Wrappers
    int read(void *buffer, size_t size, size_t offset);
    int write(const void *buffer, size_t size, size_t offset);
    int set_attributes(vnode_attributes_t *attrs);
    dentry_t *lookup(const char *name);
    int get_listing(dentry_t *&out, size_t offset, size_t limit);
    int creat(const char *name, uint16_t mode);
    int mkdir(const char *name, uint16_t mode);
    
    int unlink(const char *child);
    int rmdir(const char *child);

    // Reference counting
    void open();
    void close();

    vnode_t();
};