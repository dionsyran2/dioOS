#include <filesystem/memfs/memfs.h>
#include <memory/heap.h>
#include <math.h>
#include <memory.h>
#include <kerrno.h>
#include <cstr.h>

int expand_file(filesystems::memfs::memfs_file_t* file, uint64_t new_size){
    if (new_size <= file->buffer_size) return 0;
    
    char* new_buffer = (char*)malloc(new_size);
    
    if (!new_buffer) return -ENOMEM;

    memset(new_buffer + file->buffer_size, 0, new_size - file->buffer_size);

    if (file->buffer){
        memcpy(new_buffer, file->buffer, file->buffer_size);
        free(file->buffer);
    }

    file->buffer = new_buffer;
    file->buffer_size = new_size;
    return 0;
}


int memfs_read(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    if (this_node->type == VDIR) return -EISDIR;

    filesystems::memfs::memfs_file_t* file = (filesystems::memfs::memfs_file_t*)this_node->file_identifier;
    
    uint64_t rflags = spin_lock(&file->file_lock);

    if (offset >= file->buffer_size) {
        spin_unlock(&file->file_lock, rflags);
        return EOF; // We have reached the end!
    }

    uint64_t remaining = file->buffer_size - offset;
    uint64_t to_read = min(remaining, length);

    memcpy(buffer, file->buffer + offset, to_read);

    spin_unlock(&file->file_lock, rflags);
    return to_read;
}

char* memfs_read_link(vnode_t* this_node){
    if (this_node->type != VLNK) return nullptr;

    int size = this_node->size;
    char *buffer = (char *)malloc(size + 1);
    this_node->read(0, size, buffer);
    buffer[size] = '\0';

    return buffer;
}


int memfs_write(uint64_t offset, uint64_t length, const void* buffer, vnode_t* this_node){
    if (this_node->type == VDIR) return -EISDIR;

    filesystems::memfs::memfs_file_t* file = (filesystems::memfs::memfs_file_t*)this_node->file_identifier;
    
    uint64_t rflags = spin_lock(&file->file_lock);

    uint64_t total_size = offset + length;

    if (file->buffer_size < total_size){
        int ret = expand_file(file, total_size);
        if (ret != 0) {
            spin_unlock(&file->file_lock, rflags);
            return ret;
        }
    }

    memcpy(file->buffer + offset, buffer, length);

    this_node->size = file->buffer_size;
    spin_unlock(&file->file_lock, rflags);

    return length;
}

int memfs_truncate(uint64_t length, vnode_t* this_node){
    if (this_node->type == VDIR) return -EISDIR;
    
    filesystems::memfs::memfs_file_t* file = (filesystems::memfs::memfs_file_t*)this_node->file_identifier;
    
    if (file->buffer_size < length) return expand_file(file, length);

    uint64_t rflags = spin_lock(&file->file_lock);

    char* new_buffer = (char*)malloc(length);

    if (!new_buffer){
        spin_unlock(&file->file_lock, rflags);
        return -ENOMEM;
    }

    if (file->buffer) {
        memcpy(new_buffer, file->buffer, length);
        free(file->buffer);
    }

    file->buffer = new_buffer;
    file->buffer_size = length;
    
    this_node->size = file->buffer_size;
    spin_unlock(&file->file_lock, rflags);
    return 0;
}

int memfs_cleanup(vnode_t* this_node){
    filesystems::memfs::memfs_file_t* file = (filesystems::memfs::memfs_file_t*)this_node->file_identifier;
    if (file->buffer) free(file->buffer);

    delete file;
    return 0;
}

int memfs_unlink(vnode_t* this_node){
    filesystems::memfs::memfs_file_t* file = (filesystems::memfs::memfs_file_t*)this_node->file_identifier;
    file->to_be_removed = true;

    this_node->close(); // Close that reference we opened when we created this file.
    if (this_node->parent) this_node->parent->_remove_child(this_node);

    if (this_node->ref_count == 0) {
        memfs_cleanup(this_node);
        delete this_node;
    }

    return 0;
}

int memfs_rmdir(vnode_t* this_node){
    for (vnode_t* child = this_node->children; child != nullptr;){
        vnode_t *next = child->next;
        if (child->type == VDIR) {
            child->rmdir();
        } else {
            child->unlink();
        }
        child = next;
    }

    memfs_unlink(this_node);
    return 0;
}

int memfs_close(vnode_t* this_node){
    return 0;
}


int memfs_mkdir(char* name, vnode_t* this_node);
int memfs_creat(char* name, vnode_t* this_node);

vnode_ops_t memfs_ops = {
    .read_link = memfs_read_link,
    .read = memfs_read,
    .write = memfs_write,
    .truncate = memfs_truncate,
    .mkdir = memfs_mkdir,
    .creat = memfs_creat,
    .unlink = memfs_unlink,
    .rmdir = memfs_rmdir,
    .close = memfs_close,
    .cleanup = memfs_cleanup,
};


int memfs_mkdir(char* name, vnode_t* this_node){
    if (this_node->type != VDIR) return -ENOTDIR;

    // Check if the child exists
    vnode_t* found;
    if (this_node->find_file(name, &found) != -ENOENT){
        found->close();
        return -EEXIST;
    }

    vnode_t* node = new vnode_t(VDIR);

    if (!node) return -ENOMEM;

    filesystems::memfs::memfs_file_t* file_info = new filesystems::memfs::memfs_file_t;

    if (!file_info) {
        delete node;
        return -ENOMEM;
    }

    file_info->buffer = nullptr;
    file_info->buffer_size = 0;
    file_info->file_lock = 0;
    file_info->to_be_removed = false;

    node->file_identifier = (uint64_t)file_info;
    node->do_not_cache = true;

    strcpy(node->name, name);
    memcpy(&node->file_operations, &memfs_ops, sizeof(node->file_operations));

    this_node->_add_child(node);
    node->open(); // Keep it open to prevent it from being deleted
    return 0;
}

int memfs_creat(char* name, vnode_t* this_node){
    if (this_node->type != VDIR) return -ENOTDIR;
    
    // Check if the child exists
    vnode_t* found;
    if (this_node->find_file(name, &found) != -ENOENT){
        found->close();
        return -EEXIST;
    }

    vnode_t* node = new vnode_t(VREG);

    if (!node) return -ENOMEM;

    filesystems::memfs::memfs_file_t* file_info = new filesystems::memfs::memfs_file_t;

    if (!file_info) {
        delete node;
        return -ENOMEM;
    }

    file_info->buffer = nullptr;
    file_info->buffer_size = 0;
    file_info->file_lock = 0;
    file_info->to_be_removed = false;

    node->file_identifier = (uint64_t)file_info;
    node->do_not_cache = true;

    strcpy(node->name, name);
    memcpy(&node->file_operations, &memfs_ops, sizeof(node->file_operations));

    this_node->_add_child(node);
    node->open(); // Keep it open to prevent it from being deleted
    return 0;
}

namespace filesystems{
    namespace memfs{
        vnode_t* create_memfs(){
            vnode_t* node = new vnode_t(VDIR);

            memfs_file_t* file_info = new memfs_file_t;
            file_info->buffer = nullptr;
            file_info->buffer_size = 0;
            file_info->file_lock = 0;
            file_info->to_be_removed = false;

            node->file_identifier = (uint64_t)file_info;
            node->do_not_cache = true;

            memcpy(&node->file_operations, &memfs_ops, sizeof(node->file_operations));
            return node;
        }

        vnode_t *create_abstract_file(){
            vnode_t* node = new vnode_t(VREG);

            if (!node) return nullptr;

            filesystems::memfs::memfs_file_t* file_info = new filesystems::memfs::memfs_file_t;

            if (!file_info) {
                delete node;
                return nullptr;
            }

            file_info->buffer = nullptr;
            file_info->buffer_size = 0;
            file_info->file_lock = 0;
            file_info->to_be_removed = false;

            node->file_identifier = (uint64_t)file_info;
            node->do_not_cache = true;
            memcpy(&node->file_operations, &memfs_ops, sizeof(node->file_operations));

            return node;
        }
    }
}