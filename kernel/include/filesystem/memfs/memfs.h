#pragma once
#include <stdint.h>
#include <stddef.h>

#include <filesystem/vfs/vfs.h>

namespace filesystems{
    namespace memfs{
        struct memfs_file_t{
            char* buffer;
            size_t buffer_size;
            spinlock_t file_lock;
            bool to_be_removed;
        };

        vnode_t* create_memfs();
    }
}