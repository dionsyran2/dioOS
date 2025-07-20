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

int WritePipe(vnode_t* pipe, const char* data, size_t length);
char* ReadPipe(vnode_t* pipe, size_t* length);
vnode_t* CreatePipe(const char* name, vnode_t* parent);
void ClosePipe(vnode_t* pipe);

int delete_pipe_data(vnode_t* pipe, size_t length);