#include <drivers/filesystems/ramfs/node.h>
#include <drivers/filesystems/ramfs/dentry.h>
#include <memory/heap.h>
#include <memory.h>
#include <math.h>
#include <cstr.h>
#include <kerrno.h>

namespace ramfs {

    void rfs_vnode_t::_resize_buffer(size_t new_size) {
        if (this->buffer_size >= new_size) return;

        void *new_buffer = malloc(new_size);
        if (!new_buffer) return;
        memset(new_buffer, 0, new_size);

        if (this->buffer) {
            memcpy(new_buffer, this->buffer, this->buffer_size);
            free(this->buffer);
        }

        this->buffer = new_buffer;
        this->buffer_size = new_size;
    }

    int rfs_vnode_t::truncate(size_t size) {
        uint64_t rflags = spin_lock(&this->lock);

        if (size > this->buffer_size) {
            this->_resize_buffer(size);
        }
        this->file_size = size;

        spin_unlock(&this->lock, rflags);
        return 0;
    }

    int rfs_vnode_t::write(const void *buffer, size_t size, size_t offset) {
        if (!buffer || size == 0) return 0;

        uint64_t rflags = spin_lock(&this->lock);
        size_t total_size = offset + size;

        if (total_size > this->buffer_size) {
            this->_resize_buffer(total_size);
        }

        memcpy((char*)this->buffer + offset, buffer, size);

        this->file_size = max(this->file_size, total_size);

        spin_unlock(&this->lock, rflags);
        return size;
    }

    int rfs_vnode_t::read(void *buffer, size_t size, size_t offset) {
        if (!buffer || size == 0) return 0;

        uint64_t rflags = spin_lock(&this->lock);

        if (offset >= this->file_size) {
            spin_unlock(&this->lock, rflags);
            return 0;
        }

        size_t length = min(this->file_size - offset, size);

        memcpy(buffer, (char*)this->buffer + offset, length);

        spin_unlock(&this->lock, rflags);
        return length;
    }

    rfs_dentry_t *rfs_vnode_t::lookup(const char *name) {
        if (!name) return nullptr;

        this->children.lock();

        rfs_dentry_t *ret = nullptr;

        for (int i = 0; i < this->children.size(); i++) {
            rfs_dentry_t *e = this->children.get(i);
            if (!e) continue;

            if (strcmp(e->name, name) == 0) {
                ret = e;
                break;
            }
        }

        this->children.unlock();
        return ret;
    }

    void rfs_vnode_t::creat(const char *name, int inode) {
        rfs_dentry_t *child = new rfs_dentry_t();
        strncpy(child->name, name, sizeof(child->name) - 1);
        child->name[sizeof(child->name) - 1] = '\0';
        child->type = REG;
        child->inode = inode;

        this->children.lock();
        this->children.add(child);
        this->children.unlock();
    }

    void rfs_vnode_t::mkdir(const char *name, int inode) {
        rfs_dentry_t *child = new rfs_dentry_t();
        strncpy(child->name, name, sizeof(child->name) - 1);
        child->name[sizeof(child->name) - 1] = '\0';
        child->type = DIR;
        child->inode = inode;

        this->children.lock();
        this->children.add(child);
        this->children.unlock();
    }

    int rfs_vnode_t::unlink(const char *name){
        rfs_dentry_t *child = nullptr;

        this->children.lock();
        for (int i = 0; i < this->children.size(); i++){
            rfs_dentry_t *c = this->children.get(i);
            
            if (strcmp(c->name, name)) continue;

            child = c;
            this->children.remove(i);
            break;
        }

        this->children.unlock();

        if (!child) return -ENOENT;
        
        delete child;
        return 0;
    }
}