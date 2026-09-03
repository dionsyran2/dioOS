#pragma once
#include <stdint.h>
#include <stddef.h>
#include <structures/hashmap/hashmap.h>
#include <structures/lists/linked_list.h>
#include <vfs/vnode.h>

struct vnode_cache_t{
    kstd::hashmap<uint64_t, vnode_t *> *hashmap;
    kstd::linked_list_t<vnode_t *> *lru_list; // Note: It pushes (adds) nodes to the end of the list
                                             // So the first one is the least used one

    vnode_cache_t();
    ~vnode_cache_t();

    void add(vnode_t *vnode); // Adds a vnode to the cache (WARNING, IT DOES NOT CHECK IF ITS ALREADY IN THE CACHE)
    vnode_t *fetch(uint32_t fs_id, uint32_t inode); // Searches for a vnode in the cache. If its not found it will return nullptr.
    void release(vnode_t *vnode); // Marks a vnode as released and can be safely removed from the cache.
                                  // Fetch will revert that if its successfully retrieved

    void invalidate(vnode_t *node);

    void pnp_disconnect(int fs_id);
};