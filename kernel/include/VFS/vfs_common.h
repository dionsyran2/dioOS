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
    VBAD = 6,
    VPIPE = 7
};

struct vnode;

struct vnode_ops {
    int (*open)(vnode* node);
    int (*close)(vnode* node);
    void* (*load)(size_t* cnt, vnode* this_node);
    int (*write)(const char* txt, size_t length, vnode* this_node);
    int (*create_subdirectory)(const char* fn, bool dir, vnode* this_node);
    int (*read_dir)(vnode** out_list, size_t* out_count, vnode* this_node);
    int (*iocntl)(int op, char* argp, vnode* this_node);
    // ...
};

typedef struct vnode{
    vnode* next;
    VNODE_TYPE type;
    struct vnode_ops ops;
    char name[128];
    
    vnode* parent;
    
    vnode* children[512];
    int num_of_children;

    size_t size;
    uint64_t flags;
    
    uint8_t* loaded_data;
    size_t loaded_data_size;

    bool data_available; // used for pipes, ttys etc.
    
    void* fs_data;
    void* fs_sec_data;
    void* fs_th_data;
    
    int iocntl(int op, char* argp);
    void* load(size_t* cnt);
    int write(const char* txt, size_t length);
    int mkfile(const char* fn, bool dir);
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
    bool read(uint64_t block, uint64_t block_count, void* buffer);
    bool write(uint64_t block, uint64_t block_count, const void* buffer);
} vblk_t;