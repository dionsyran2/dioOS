// drivers/filesystems/dentry.h

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <drivers/filesystems/ramfs/ramfs.h>

namespace ramfs{
    // Defines a directory entry
    // Like ext2, it represents a point
    // not the actual file
    struct rfs_dentry_t{
        rfs_type_t type;

        char name[256];

        uint32_t inode;
    };
}