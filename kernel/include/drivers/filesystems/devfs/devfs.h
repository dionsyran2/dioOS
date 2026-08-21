#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vfs/vfs.h>

struct devfs_ops_t{
    int (*read)(void *context, void *buffer, size_t size, size_t offset);
    int (*write)(void *context, const void *buffer, size_t size, size_t offset);
    int (*ioctl)(void *context, int op, char* argp);
    int (*poll)(void *context, int events, poll_table_t *pt);
};

namespace devfs{
    vnode_t *resolve_path(const char *path);
    dentry_t *resolve_path_dentry(const char *path);
    
    int mknod(const char *path, uint16_t mode, devfs_ops_t *operations, void *ctx);
}