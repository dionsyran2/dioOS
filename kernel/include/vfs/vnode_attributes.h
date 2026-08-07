#pragma once
#include <stdint.h>
#include <stddef.h>

#define VNODE_ATTR_MODE (1 << 0)
#define VNODE_ATTR_UID  (1 << 1)
#define VNODE_ATTR_GID  (1 << 2)
#define VNODE_ATTR_ATIME (1 << 3)
#define VNODE_ATTR_MTIME (1 << 4)
#define VNODE_ATTR_CTIME (1 << 5)
#define VNODE_ATTR_BTIME (1 << 6)

struct vnode_attributes_t {
    uint32_t valid; // Bitmask of which fields to update

    uint32_t mode; // File type + POSIX Permissions (e.g., S_IRUSR | S_IWUSR)
    int uid;
    int gid;

    uint64_t atime; // Last access time (Updates on read())
    uint64_t mtime; // Last modification time (Updates on write())
    uint64_t ctime; // Status Change time (Updated when the file's metadata changes)
    uint64_t btime; // Birth time (Creation Time)
};