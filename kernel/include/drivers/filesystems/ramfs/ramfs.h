// drivers/filesystems/ramfs.h

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <structures/trees/avl_tree.h>

struct dentry_t;

namespace ramfs{
    enum rfs_type_t{
        INVALID = 0,
        DIR,
        REG,
        DEV,
        SOCK,
        LNK,
    };
    
    struct rfs_vnode_t;

    struct __ramfs{
        kstd::avl_tree_t<rfs_vnode_t*> vnode_tree;
        int current_inode_id = 1;
        int fs_id;

        int allocate_vnode(rfs_type_t type, uint16_t mode);
        rfs_vnode_t *get_node(int identifier);
        void free_vnode(int inode);
    };

    dentry_t *create_fs();
}