// drivers/filesystems/node.h

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/spinlock/spinlock.h>
#include <structures/lists/linked_list.h>
#include <drivers/filesystems/ramfs/ramfs.h>
#include <vfs/vnode_attributes.h>


namespace ramfs{
    // This represents the actual file (the contents)

    struct rfs_dentry_t;
    struct rfs_vnode_t{
        // File metadata
        rfs_type_t type = REG;
        size_t file_size = 0;
        int nlink = 1;

        vnode_attributes_t attributes;

        // Data Storage
        kstd::linked_list_t<rfs_dentry_t *> children;

        void *buffer = nullptr;
        size_t buffer_size = 0;

        spinlock_t lock = 0;

        // Methods
        int write(const void *buffer, size_t size, size_t offset);
        int read(void *buffer, size_t size, size_t offset);
        int truncate(size_t size);

        // Directory Methods
        rfs_dentry_t *lookup(const char *name);
        void creat(const char *name, int inode);
        void mkdir(const char *name, int inode);
        void mklink(const char *name, int inode);

        int unlink(const char *name);

        void _resize_buffer(size_t new_size);
    };
}