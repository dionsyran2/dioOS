#include <vfs/vnode_cache.h>

void vnode_cache_t::add(vnode_t *vnode){
    uint64_t key = (uint64_t)vnode->fs_id;
    key <<= 32;
    key |= vnode->inode;

    this->hashmap->insert(key, vnode);
}

vnode_t *vnode_cache_t::fetch(uint32_t fs_id, uint32_t inode){
    uint64_t key = (uint64_t)fs_id;
    key <<= 32;
    key |= inode;

    vnode_t *out = nullptr;
    bool res = this->hashmap->search(key, out);

    if (!res || !out) return nullptr;

    if (out->kflag_bitfield & VNODE_KFLAG_CACHE_RELEASED) {
        this->lru_list->lock();
        
        for (int i = 0; i < this->lru_list->size(); i++){
            if (this->lru_list->get(i) != out) continue;

            this->lru_list->remove(i);
            break;
        }

        out->kflag_bitfield &= ~(uint32_t)VNODE_KFLAG_CACHE_RELEASED;
        this->lru_list->unlock();
    }

    return out;
}

void vnode_cache_t::release(vnode_t *vnode){
    if (vnode->kflag_bitfield & VNODE_KFLAG_CACHE_RELEASED) return;
    vnode->kflag_bitfield |= VNODE_KFLAG_CACHE_RELEASED;
    
    this->lru_list->lock();
    this->lru_list->add(vnode);
    this->lru_list->unlock();

    /* LRU Eviction */
    if (this->lru_list->size() > 256) {
        vnode_t *victim = this->lru_list->get(0);
        this->lru_list->remove(0);

        // Calculate key to scrub it from the global lookup map
        uint64_t victim_key = (uint64_t)victim->fs_id;
        victim_key <<= 32;
        victim_key |= victim->inode;

        // Erase map index so future fetches look to the disk driver instead of empty space
        this->hashmap->remove(victim_key);

        delete victim;
    }

    this->lru_list->unlock();
}

void vnode_cache_t::invalidate(vnode_t *node) {
    if (!node) return;

    uint64_t key = (uint64_t)node->fs_id;
    key <<= 32;
    key |= node->inode;

    // Remove from global hash map so no new path lookups can find it
    this->hashmap->remove(key);

    // If it is sitting in the LRU eviction list, scrub it out
    if (node->kflag_bitfield & VNODE_KFLAG_CACHE_RELEASED) {
        this->lru_list->lock();
        for (int i = 0; i < this->lru_list->size(); i++) {
            if (this->lru_list->get(i) == node) {
                this->lru_list->remove(i);
                break;
            }
        }
        this->lru_list->unlock();
        node->kflag_bitfield &= ~(uint32_t)VNODE_KFLAG_CACHE_RELEASED;
    }

    // Mark the node as dead/unlinked
    node->kflag_bitfield |= VNODE_KFLAG_UNLINKED;
}

void vnode_cache_t::pnp_disconnect(int fs_id) {
    kstd::linked_list_t<vnode_t*> dead_nodes;

    this->hashmap->for_each([&](uint64_t key, vnode_t* vnode) {
        if ((key >> 32) == (uint32_t)fs_id) {
            dead_nodes.add(vnode);
        }
    });

    for (int i = 0; i < dead_nodes.size(); i++) {
        vnode_t* node = dead_nodes.get(i);
        
        node->kflag_bitfield |= VNODE_KFLAG_DEAD; 
        
        this->invalidate(node);
    }
}

vnode_cache_t::vnode_cache_t(){
    this->lru_list = new kstd::linked_list_t<vnode_t *>();
    this->hashmap = new kstd::hashmap<uint64_t, vnode_t *>(128);
}

vnode_cache_t::~vnode_cache_t(){
    delete this->lru_list;
    delete this->hashmap;
}

