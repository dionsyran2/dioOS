#pragma once
#include <stdint.h>
#include <stddef.h>
#include <VFS/vfs.h>
#include <scheduling/lock/spinlock.h>

struct pipe_data{
    char* data;
    int buffer_size;
    int offset_in_buffer;
    spinlock_t lock;
};

int64_t WritePipe(vnode_t* pipe, const void* data, size_t length);
int64_t ReadPipe(vnode_t* pipe, void* buffer, size_t cnt);
vnode_t* CreatePipe(const char* name, vnode_t* parent);
void ClosePipe(vnode_t* pipe);

int delete_pipe_data(vnode_t* pipe, size_t length);