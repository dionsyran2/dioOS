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
    int64_t (*read)(void* buffer, size_t cnt, size_t offset, vnode* this_node);
    int64_t (*write)(const void* buffer, size_t cnt, size_t offset, vnode* this_node);
    int64_t (*truncate)(uint64_t size, vnode* this_node);
    int (*mkdir)(const char* fn, vnode* this_node);
    int (*creat)(const char* fn, vnode* this_node);
    int (*read_dir)(vnode** out_list, vnode* this_node);
    int (*iocntl)(int op, char* argp, vnode* this_node);
    int (*save_entry)(vnode* this_node);
    // ...
};

typedef struct vnode{
    vnode* next;
    vnode* parent;
    VNODE_TYPE type;
    struct vnode_ops ops;

    char name[256];
    char pathname[512];

    vnode* static_children;
    uint16_t child_cnt;

    uint16_t perms;

    uint16_t uid;
    uint16_t gid;

    uint64_t size;
    uint64_t last_accessed;
    uint64_t last_modified;
    uint64_t creation_time;

    uint32_t inode;
    uint32_t fs_inode;

    uint32_t flags;

    uint8_t* cache;
    uint8_t* write_cache;
    uint64_t write_cache_size;
    uint64_t write_cache_size_in_pages;
    uint64_t write_cache_offset;

    bool data_read;
    bool data_write;
    bool is_tty;

    bool edited;
    bool bypass_write_cache;
    
    bool is_static;
    
    uint16_t ref_cnt;

    void* misc_data[4];

    char link_target[512];

    int open();
    int close();
    int64_t read(void* buffer, size_t cnt, size_t offset = 0);
    int64_t write(const void* buffer, size_t cnt, size_t offset = 0);
    int64_t truncate(size_t size);
    int iocntl(int op, char* argp);
    int mkfile(const char* fn, bool dir);
    int read_dir(vnode** out);
    int save_entry();
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