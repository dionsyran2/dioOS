#include <vfs/vnode.h>
#include <vfs/vfs.h>
#include <kerrno.h>


vnode_t::vnode_t(){

}


void vnode_t::open(){
    __atomic_fetch_add(&this->ref_count, 1, __ATOMIC_SEQ_CST);
}


void vnode_t::close() {
    if (__atomic_fetch_sub(&this->ref_count, 1, __ATOMIC_SEQ_CST) == 1) {
        // If all hard links were deleted (nlink == 0), reclaim storage!
        if (this->nlink == 0) {
            if (this->operations && this->operations->evict_inode) {
                this->operations->evict_inode(this); // Frees blocks AND inode!
            }
        }

        // Remove from VFS cache and delete the vnode object itself
        vfs::__release_vnode(this);
    }
}

int vnode_t::read(void *buffer, size_t size, size_t offset){
    if (S_ISDIR(this->attributes.mode)) return -EISDIR;

    if (!this->operations || !this->operations->read) return -ENOSYS;

    this->klock.lock();
    int ret = this->operations->read(this, buffer, size, offset);
    this->klock.unlock();

    return ret;
}

int vnode_t::write(const void *buffer, size_t size, size_t offset){
    if (S_ISDIR(this->attributes.mode)) return -EISDIR;

    if (!this->operations || !this->operations->write) return -ENOSYS;

    this->klock.lock();
    int ret = this->operations->write(this, buffer, size, offset);
    this->klock.unlock();

    return ret;
}

int vnode_t::set_attributes(vnode_attributes_t *attrs){
    if (!this->operations || !this->operations->set_attributes) return -ENOSYS;

    // Acquire the lock
    this->klock.lock();

    // Call the fs operation
    int ret = this->operations->set_attributes(this, attrs);

    // Free the lock
    this->klock.unlock();

    return ret;
}

dentry_t *vnode_t::lookup(const char *name){
    if (!this->operations || !this->operations->lookup) {return nullptr;}

    this->klock.lock();
    dentry_t *r = this->operations->lookup(this, name);
    this->klock.unlock();


    return r;
}

int vnode_t::get_listing(dentry_t *&out, size_t offset, size_t limit){
    if (!this->operations || !this->operations->get_listing) return 0;

    this->klock.lock();
    
    int r = this->operations->get_listing(this, out, offset, limit);
    
    this->klock.unlock();

    return r;
}

int vnode_t::creat(const char *name, uint16_t mode){
    if (!S_ISDIR(this->attributes.mode)) return -ENOTDIR;

    if (!this->operations || !this->operations->creat) return -EROFS;

    dentry_t *lookup = this->lookup(name);
    if (lookup){
        delete lookup; // Because this is an out-of-cache reference we have to delete it
        return -EEXIST;
    }

    this->klock.lock();
    
    int r = this->operations->creat(this, name, mode);
    
    this->klock.unlock();

    return r;
}

int vnode_t::mkdir(const char *name, uint16_t mode){
    if (!S_ISDIR(this->attributes.mode)) return -ENOTDIR;

    if (!this->operations || !this->operations->mkdir) return -EROFS;

    dentry_t *lookup = this->lookup(name);
    if (lookup){
        delete lookup; // Because this is an out-of-cache reference we have to delete it
        return -EEXIST;
    }
    
    this->klock.lock();
    
    int r = this->operations->mkdir(this, name, mode);
    
    this->klock.unlock();

    return r;
}

int vnode_t::poll(int events, poll_table_t *pt){
    if (!this->operations || !this->operations->poll) return -EOPNOTSUPP;

    return this->operations->poll(this, events, pt);
}

int vnode_t::unlink(const char *child) {
    if (!S_ISDIR(this->attributes.mode)) return -ENOTDIR;
    if (!this->operations || !this->operations->unlink) return -EROFS;

    dentry_t *child_dentry = this->lookup(child);
    vnode_t *child_vnode = child_dentry ? vfs::_get_vnode(child_dentry) : nullptr;

    
    this->klock.lock();
    int ret = this->operations->unlink(this, child); // Driver removes name entry & decrements nlink
    this->klock.unlock();

    // If unlink succeeded, handle cache invalidation
    if (ret == 0 && child_vnode) {
        // Decrement in-memory nlink count
        if (child_vnode->nlink > 0) {
            child_vnode->nlink--;
        }

        // If no hard links remain on disk, invalidate from cache so new open() calls miss
        if (child_vnode->nlink == 0) {
            vfs::__invalidate_vnode_cache(child_vnode);
        }
    }

    // Cleanup local references
    if (child_vnode) child_vnode->close();
    if (child_dentry) child_dentry->unref();

    return ret;
}

int vnode_t::ioctl(int op, char* argp){
    if (!this->operations || !this->operations->ioctl) return -EOPNOTSUPP;

    return this->operations->ioctl(this, op, argp);
}