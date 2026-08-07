#include <vfs/dentry.h>
#include <string.h>

dentry_t::dentry_t(){
    this->ref_count = 0;
    this->mounted_root = nullptr;
    this->parent = nullptr;
}

dentry_t::dentry_t(const char* name, uint32_t inode, uint32_t fs_id){
    strncpy(this->name, name, sizeof(this->name) / sizeof(char));

    this->inode = inode;
    this->fs_id = fs_id;

    this->ref_count = 0;
    this->mounted_root = nullptr;
    this->parent = nullptr;
}

dentry_t::~dentry_t(){

}

void dentry_t::ref(){
    __atomic_fetch_add(&this->ref_count, 1, __ATOMIC_SEQ_CST);

    if (this->parent){
        this->parent->ref();
    }
}

void dentry_t::unref(){
    __atomic_fetch_sub(&this->ref_count, 1, __ATOMIC_SEQ_CST);
    if (this->parent){
        this->parent->unref();
    }
}

// Traversal & Cache Utilities
dentry_t* dentry_t::lookup_child(const char* name){
    this->children.lock();

    for (int i = 0; i < this->children.size(); i++){
        dentry_t *child = this->children.get(i);

        if (strcmp(name, child->name)) continue;

        this->children.unlock();
        return child;
    }

    this->children.unlock();
    return nullptr;
}

void dentry_t::add_child(dentry_t* child){
    this->children.lock();

    this->children.add(child);
    child->parent = this;
    
    this->children.unlock();
}

vnode_t *dentry_t::fetch_vnode(){
    if (this->__fs_fetch_vnode == nullptr) return nullptr;


    return this->__fs_fetch_vnode(this);
}