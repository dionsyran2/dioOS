#pragma once
#include <filesystem/vfs/vfs.h>
#include <scheduling/spinlock/spinlock.h>

struct pipe_t{
    vnode_t *write_node;
    vnode_t *read_node;
   
    void *buffer;
    size_t buffer_size;
    size_t write_offset;

    spinlock_t pipe_lock;

    uint8_t ref_cnt;
};

pipe_t *create_pipe();