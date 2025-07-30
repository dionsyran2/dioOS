/*******************************************************************/
/* This file contains definitions for the vfs (Virtual File System)*/
/*******************************************************************/

#pragma once
#include <stdint.h>
#include <stddef.h>

#define VFS_NO_CACHE (1 << 0) // disables caching for that vnode
#define VFS_RO (1 << 1) // read only


enum VNODE_TYPE{
    VREG = 0,
    VDIR = 1,
    VLNK = 2,
    VCHR = 3,
    VBLK = 4,
    VSOC = 5,
    VBAD = 6,
    VFIFO = 7
};

struct vnode;

struct vnode_ops {
    int (*open)(vnode* node);
    int (*close)(vnode* node);
    void* (*read)(size_t* cnt, vnode* this_node);
    int (*write)(const char* txt, size_t length, vnode* this_node);
    int (*mkdir)(const char* fn, vnode* this_node);
    int (*creat)(const char* fn, vnode* this_node);
    int (*read_dir)(vnode** out_list, size_t* out_count, vnode* this_node);
    int (*iocntl)(int op, char* argp, vnode* this_node);
    //save_on_disk()
    // ...
};

typedef struct vnode{
    vnode* next;
    vnode* parent;
    VNODE_TYPE type;
    struct vnode_ops ops;
    char name[256];
        
    vnode* children;
    uint16_t child_cnt;

    uint16_t perms;

    uint16_t uid;
    uint16_t gid;

    uint32_t size;
    uint64_t last_accessed;
    uint64_t last_modified;
    uint64_t creation_time;

    uint32_t inode;
    uint32_t fs_inode;

    uint32_t flags;
    
    bool data_read;
    bool data_write;
    bool is_tty;

    uint8_t* data;
    uint32_t data_size;

    uint16_t ref_cnt;

    void* misc_data[4];

    char link_target[512];

    int open();
    int close();
    int read(size_t* cnt, void** buffer);
    int iocntl(int op, char* argp);
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