/*******************************************************************/
/* This file contains definitions for the vfs (Virtual File System)*/
/*******************************************************************/

#pragma once
#include <stdint.h>
#include <stddef.h>

enum VNODE_TYPE{
    VREG = 0,
    VDIR = 1,
    VLNK = 2,
    VCHR = 3,
    VBLK = 4,
    VSOC = 5,
    VBAD = 6
};

struct vnode;

struct vnode_ops {
    int (*open)(vnode* node);
    int (*close)(vnode* node);
    void* (*load)(size_t* cnt, vnode* this_node);
    int (*write)(const char* txt, size_t length, vnode* this_node);
    int (*create_subdirectory)(const char* fn, bool dir, vnode* this_node);
    int (*read_dir)(vnode** out_list, size_t* out_count, vnode* this_node);
    // ...
};

typedef struct vnode{
    vnode* next;
    VNODE_TYPE type;
    char name[64];

    struct vnode_ops ops;
    
    vnode* parent;
    
    vnode* children[124];
    int num_of_children;

    void* fs_data;
    void* fs_sec_data;
    void* fs_th_data;
} vnode_t;

struct vblk;

struct vblk_ops{
    bool (*read)(uint64_t block, uint32_t block_count, void* buffer, vblk* blk);
    bool (*write)(uint64_t block, uint32_t block_count, const void* buffer, vblk* blk);
};

typedef struct vblk{
    vnode node;
    size_t block_size;
    size_t block_count;
    
    vblk_ops blk_ops;
    uint8_t num_of_partitions; // The number of partitions mounted... used just to keep track of the naming
    void* drv_data; // driver data
} vblk_t;