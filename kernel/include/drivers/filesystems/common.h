#pragma once
#include <stdint.h>
#include <stddef.h>

struct vnode_t; // Forward declaration

// A struct that defines a filesystem entry
struct filesystem_entry_t {
    const char name[64];

    vnode_t *(*parse_filesystem)(vnode_t *disk); // This will try to read a filesystem of a disk
                                                 // If it successfully reads one, it will return a root node
};

void register_filesystem(filesystem_entry_t *filesystem);