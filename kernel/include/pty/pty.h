#pragma once
#include <stdint.h>
#include <stddef.h>
#include <filesystem/vfs/vfs.h>
#include <syscalls/files/termios.h>
#include <scheduling/spinlock/spinlock.h>

struct pty_pipe{
    void *buffer;
    size_t buffer_size;
    size_t offset;

    spinlock_t lock;

    int read(int size, void *buffer, bool *empty);
    int write(int size, const void *buffer);
};

struct pty_pair {
    int id;
    pty_pipe to_master;
    pty_pipe to_slave;

    winsize windowsize;

    vnode_t *master_node;
    vnode_t *slave_node;

    int foreground_pgrp;

    struct termios conf;
};


void initialize_pty_multiplexer();