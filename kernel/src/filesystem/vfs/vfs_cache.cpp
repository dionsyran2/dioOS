#include <filesystem/vfs/vfs_cache.h>
#include <memory/heap.h>
#include <cstr.h>
#include <math.h>
#include <structures/trees/avl_tree.h>

namespace vfs {

    void *cache_t::_cache(uint64_t offset, uint64_t length, vnode_t *node){
        if (offset >= node->size) {
            return nullptr;
        } 

        if (offset + length > node->size){
            length = node->size - offset;
        }

        if (length == 0) {
            return nullptr;
        }

        // Case 1: Nothing is cached
        if (this->data == nullptr){
            this->offset = offset;
            this->data = malloc(length);

            int bytes_read = node->file_operations.read(offset, length, this->data, node);
            
            if (bytes_read <= 0) {
                free(this->data);
                this->data = nullptr;
                return nullptr;
            }
            
            this->length = bytes_read;
        }
        
        if (this->offset + this->length < offset + length) { // Case 2: Expand forwards
            uint64_t newend = offset + length;
            uint64_t oldend = this->offset + this->length;
            uint64_t newlength = newend - this->offset;

            void *newbuf = malloc(newlength);
            memcpy(newbuf, this->data, this->length);
            free(this->data);
            
            int bytes_read = node->file_operations.read(oldend, newend - oldend, (uint8_t*)newbuf + this->length, node);
            
            this->data = newbuf;
            this->length += (bytes_read > 0) ? bytes_read : 0; 
        }

        if (this->offset > offset){ // Case 3: Expand backwards
            uint64_t gap = this->offset - offset;
            uint64_t new_size = this->length + gap;

            void *newbuf = malloc(new_size);
            memcpy((uint8_t*)newbuf + gap, this->data, this->length);

            int bytes_read = node->file_operations.read(offset, gap, newbuf, node);

            free(this->data);
            this->data = newbuf;
            this->length += (bytes_read > 0) ? bytes_read : 0;
            this->offset = offset;
        }

        uint64_t diff = offset - this->offset;

        return (uint8_t*)this->data + diff;
    }

    int cache_t::read(uint64_t offset, uint64_t length, void *buffer, vnode_t *node){
        if (length == 0) return 0; // Prevent probe reads from returning -1
        if (offset >= node->size) return 0;
        
        if (offset + length > node->size){
            length = node->size - offset;
        }

        void *cache = this->_cache(offset, length, node);
        if (cache == nullptr) return -1;

        uint64_t diff = offset - this->offset;
        uint64_t valid_data = this->length - diff;
        
        uint64_t copy_len = (length < valid_data) ? length : valid_data;

        memcpy(buffer, cache, copy_len);

        return copy_len;
    }

    int cache_t::write(uint64_t offset, uint64_t length, const void *buffer, vnode_t *node){
        if (offset > node->size) return 0;
        if (offset + length > node->size){
            length = node->size - offset;
        }

        void *cache = this->_cache(offset, length, node);
        memcpy(cache, buffer, length);

        return length;
    }
    

    cache_t::cache_t(){
        this->data = nullptr;
        this->offset = 0;
        this->length = 0;
    }

    cache_t::~cache_t(){
        // sync any changes and delete any memory allocated!
    }

    kstd::avl_tree_t<cache_t *> *cache_tree;

    int cache_read(uint64_t offset, uint64_t length, void *buffer, vnode_t *node){
        if (node->do_not_cache || node->type != VREG) return node->file_operations.read(offset, length, buffer, node);

        if (cache_tree == nullptr) {
            cache_tree = new kstd::avl_tree_t<cache_t *>();
        }

        uint64_t key = ((uint64_t)node->dev_id << 32) | node->inode;

        cache_t *cache = cache_tree->search(key);
        if (!cache){
            cache = new cache_t();
            cache_tree->insert(key, cache);
        }

        int ret = cache->read(offset, length, buffer, node);
        return ret;
    }

    int cache_write(uint64_t offset, uint64_t length, const void *buffer, vnode_t *node){
        int write = node->file_operations.write(offset, length, buffer, node);
        if (node->do_not_cache || write < 0 || node->type != VREG) return write; 

        if (cache_tree == nullptr) {
            cache_tree = new kstd::avl_tree_t<cache_t *>();
        }

        uint64_t key = ((uint64_t)node->dev_id << 32) | node->inode;

        cache_t *cache = cache_tree->search(key);

        if (!cache){
            cache = new cache_t();
            cache_tree->insert(key, cache);
        }

        return cache->write(offset, length, buffer, node);
    }
}