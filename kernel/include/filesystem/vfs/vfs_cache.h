#pragma once
#include <stdint.h>
#include <stddef.h>
#include <structures/lists/linked_list.h>
#include <filesystem/vfs/vfs.h>
#include <scheduling/spinlock/spinlock.h>
struct vnode_t;

namespace vfs{
    /*struct cache_segment_t{
        uint64_t offset;
        uint64_t size;

        void *data;
    };*/
    

    class cache_t{
        public:

        private:
        uint64_t offset;
        uint64_t length;
        void *data;

        spinlock_t lock = 0;

        public:
        cache_t();
        ~cache_t();

        int read(uint64_t offset, uint64_t length, void *buffer, vnode_t *node);
        int write(uint64_t offset, uint64_t length, const void *buffer, vnode_t *node);
        private:
        void *_cache(uint64_t offset, uint64_t length, vnode_t *node);

    };

    int cache_read(uint64_t offset, uint64_t length, void *buffer, vnode_t *node);
    int cache_write(uint64_t offset, uint64_t length, const void *buffer, vnode_t *node);
}