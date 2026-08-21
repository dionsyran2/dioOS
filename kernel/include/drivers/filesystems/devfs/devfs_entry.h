#pragma once
#include <stdint.h>
#include <stddef.h>
#include <structures/lists/linked_list.h>
#include <drivers/filesystems/devfs/devfs.h>

namespace devfs{
    struct devfs_entry_t{
        char name[64];

        vnode_attributes_t attributes;

        // DIR ONLY
        kstd::linked_list_t<devfs_entry_t *> *children;

        // BLK/CHR ONLY
        devfs_ops_t *operations;
        void *operation_context;
        
        int read(void *buffer, size_t size, size_t offset);
        int write(const void *buffer, size_t size, size_t offset);
        int ioctl(int op, char* argp);
        int poll(int events, poll_table_t *pt);

        // Generic
        int inode;

        // @param name The name of the node (file)
        // @param type The type of the node (DEVFS_DIR, DEVFS_BLK, DEVFS_CHR)
        // @param context Context to be passed to the operations. Only required if type != DEVFS_DIR
        // @param operations The operations pointer structure
        devfs_entry_t(char *name, uint16_t mode, void *context, devfs_ops_t *operations);
        ~devfs_entry_t();
    };
}
