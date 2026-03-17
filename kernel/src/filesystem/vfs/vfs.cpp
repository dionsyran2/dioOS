#include <filesystem/vfs/vfs.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <cstr.h>
#include <kerrno.h>
#include <memory/heap.h>
#include <kstdio.h>
#include <filesystem/fat32/fat32.h>
#include <filesystem/ext2/ext2.h>
#include <filesystem/memfs/memfs.h>
#include <CONFIG.h>

#define MAX_SYMLINK_DEPTH 8
#define PATH_SEPARATOR "/"

void vnode_remove_from_child_list(vnode_t* parent, vnode_t* child, bool lock = true){
    // Acquire the child list lock
    uint32_t rflags;
    if (lock) rflags = spin_lock(&parent->child_list_lock);

    if (parent->children == child) {
        parent->children = child->next;
        if (lock) spin_unlock(&parent->child_list_lock, rflags);
        return;
    }

    vnode_t* prev = nullptr;
    for (vnode_t* chd = parent->children; chd != nullptr; chd = chd->next){
        if (chd == child){
            prev->next = chd->next;
            break;
        }
        prev = chd;
    }
    if (lock) spin_unlock(&parent->child_list_lock, rflags);
}

void vnode_add_to_child_list(vnode_t* parent, vnode_t* child){
    child->next = nullptr;
    if (parent->children == nullptr){
        parent->children = child;
        return;
    }

    vnode_t* last = parent->children;
    while (last->next) {
        if (last == child) {
             return;
        }
        last = last->next;
    }
    
    if (last != child) last->next = child;
}

int vnode_t::open(){
    // Atomically fetch and then increase the reference count
    uint32_t prev_ref_cnt = __atomic_fetch_add(&this->ref_count, 1, __ATOMIC_SEQ_CST);

    if (this->file_operations.open) this->file_operations.open(this);

    if (prev_ref_cnt == 0 && this->parent){
        /*bool found = false;

        // Acquire the lock
        uint32_t rflags = spin_lock(&this->parent->child_list_lock);
        
        for (vnode_t* child = this->parent->children; child != nullptr; child = child->next){
            if (child == this) {found = true; break;} // already cached
        }
        

        // If its not cached, cache it
        if (!found){
            vnode_add_to_child_list(this->parent, this);
        }

        // Release the lock
        spin_unlock(&this->parent->child_list_lock, rflags);*/

        // Increase the parent's count, so it doesn't get deleted prematurely
        this->parent->open();
    }
    
    return 0;
}

int vnode_t::close(){
    // Atomically subtract and then fetch the reference count
    int new_ref_count = __atomic_sub_fetch(&this->ref_count, 1, __ATOMIC_SEQ_CST);

    if (this->file_operations.close) this->file_operations.close(this);

    // If no one is using the file anymore...
    if (new_ref_count == 0) {
        
        // Balance the Parent Reference
        if (this->parent) {
            this->parent->close();
        }

        if (this->parent == nullptr) {
            if (this->file_operations.cleanup) this->file_operations.cleanup(this);
            delete this;
        }
    }
    return 0;
}

int vnode_t::ioctl(int op, char* argp){
    if (this->file_operations.ioctl){
        return this->file_operations.ioctl(op, argp, this);
    }

    return -ENOSYS;
}


vnode_t::vnode_t(){
    memset(this, 0, sizeof(vnode_t));
    ready_to_receive_data = true;
    data_ready_to_read = true;
    permissions = 0755;
}

vnode_t::vnode_t(vnode_type_t type){
    memset(this, 0, sizeof(vnode_t));

    this->type = type;
    ready_to_receive_data = true;
    data_ready_to_read = true;
    permissions = 0755;
}

vnode_t::~vnode_t(){

}

char* vnode_t::read_link(){
    if (this->type != VLNK) return nullptr;

    // It should not be called on a copy (Only call functions for files acquired via find_file).
    if (this->real_node) return this->real_node->read_link();

    if (file_operations.read_link != nullptr){
        return file_operations.read_link(this);
    }

    return nullptr;
}


int vnode_t::read(uint64_t offset, uint64_t length, void* buffer){
    if (this->type == VDIR) return -EISDIR;

    // It should not be called on a copy (Only call functions for files acquired via find_file).
    if (this->real_node) return this->real_node->read(offset, length, buffer);

    if (file_operations.read != nullptr){
        return file_operations.read(offset, length, buffer, this);
    }
    return 0;
}

int vnode_t::write(uint64_t offset, uint64_t length, const void* buffer){
    if (this->type == VDIR) return -EISDIR;

    // It should not be called on a copy (Only call functions for files acquired via find_file).
    if (this->real_node) return this->real_node->write(offset, length, buffer);

    if (file_operations.write != nullptr){
        return file_operations.write(offset, length, buffer, this);
    }
    return 0;
}

int vnode_t::truncate(uint64_t size){
    if (!this->file_operations.truncate) return -EOPNOTSUPP;

    // It should not be called on a copy (Only call functions for files acquired via find_file).
    if (this->real_node) return this->real_node->truncate(size);

    return this->file_operations.truncate(size, this);
}

// @warning It will return an already open vnode!
int vnode_t::find_file(const char* filename, vnode_t** ret){
    if (this->type != VDIR) return -ENOTDIR;

    // It should not be called on a copy (Only call functions for files acquired via find_file).
    if (this->real_node) return this->real_node->find_file(filename, ret);

    vnode_t* list = nullptr;
    
    int count = this->read_dir(&list);

    if (count == 0) return -ENOENT;

    uint64_t rflags = spin_lock(&child_list_lock);

    bool node_found = false;
    for (vnode_t* node = list; node != nullptr;){
        vnode_t* next = node->next;
        int r = strcmp(node->name, filename);
        
        if (r == 0){
            node_found = true;
            if (node->real_node){
                // Open the node to avoid race conditions
                node->real_node->open();
                *ret = node->real_node;
                delete node;
            } else {
                *ret = node;
                node->next = nullptr;
            }
            node = next;
            continue;
        }

        // read_dir returns a copy of the children, so we can safely delete it
        delete node;
        node = next;
    }

    spin_unlock(&child_list_lock, rflags);

    return node_found ? 0 : -ENOENT;
}

int vnode_t::read_dir(vnode_t** ret){
    // Check if its cached
    // If it is, copy the children into a newly allocated list,
    // set their real_node parameter and place them in ret
    // (To avoid any list changes corrupting the list, since the caller
    //  probably doesn't hold the list lock.)

    // If its not, call the operation read_dir provided by the fs driver.
    // Loop through the list returned and update / create the missing entries
    // in the child list

    // Loop again and check for any extra (e.g. deleted files) and remove them from the list
    // Call close() if their ref_cnt is zero, to free their memory.
    
    vnode_t *self = this->real_node ? this->real_node : this;
    *ret = nullptr;

    if (self->type != VDIR) return -ENOTDIR;


    spinlock_t *child_lock = &self->child_list_lock;

    uint64_t rflags = spin_lock(child_lock);


    // If its not cached, and we should cache it, do so.
    if (!self->directory_cached){
        // Firstly loop through the current state of the list and remove any files that shouldn't be there

        size_t child_cnt = 0;
        if (self->file_operations.read_dir) child_cnt = self->file_operations.read_dir(ret, this);

        for (vnode_t *next, *child = self->children; child != nullptr; child = next){
            next = child->next;

            // If its a virtual file (meaning it does not necessarily exist on the disk), skip it
            if (child->virtual_file) continue;

            // Otherwise check if it exists in the structure returned
            
            bool exists = false;
            for (vnode_t* a = *ret; a != nullptr; a = a->next){
                if (strcmp(a->name, child->name) == 0){
                    exists = true;
                    break;
                }
            }

            if (!exists){
                vnode_remove_from_child_list(self, child, false);
                
                if (child->ref_count == 0) {
                    child->parent = nullptr;
                    child->close();
                }
            }

        }

        // Now that the list has been cleaned, we can update it
        for (vnode_t* entry = *ret; entry != nullptr; entry = entry->next){
            vnode_t *cached_entry = nullptr;
            for (vnode_t* a = self->children; a != nullptr; a = a->next){
                if (strcmp(a->name, entry->name) == 0){
                    cached_entry = a;
                    break;
                }
            }


            if (cached_entry){
                // Update its contents
                
                cached_entry->permissions = entry->permissions;
                cached_entry->uid = entry->uid;
                cached_entry->gid = entry->gid;
                cached_entry->last_accessed = entry->last_accessed;
                cached_entry->last_modified = entry->last_modified;
                cached_entry->creation_time = entry->creation_time;
                cached_entry->size = entry->size;
                cached_entry->partition_total_size = entry->partition_total_size;
                cached_entry->io_block_size = entry->io_block_size;
                cached_entry->inode = entry->inode;
                cached_entry->fs_identifier = entry->fs_identifier;
                cached_entry->file_identifier = entry->file_identifier;
            } else {
                // Create it
                vnode_t *clone = new vnode_t(entry->type);
                memcpy(clone, entry, sizeof(vnode_t));
                clone->parent = self;
                clone->ref_count = 0;

                vnode_add_to_child_list(self, clone);
            }
        }

        // Free the list we aqcuired from the filesystem
        for (vnode_t *next, *entry = *ret; entry != nullptr; entry = next){
            next = entry->next;
            
            delete entry;
        }

        *ret = nullptr;

        self->directory_cached = true;
    }

    int cnt = 0;

    if (self->is_mounted){
        cnt = self->mount_point->read_dir(ret);
    }

    
    // Create a copy of the list

    for (vnode_t *child = self->children; child != nullptr; child = child->next){
        cnt++;

        vnode_t *clone = new vnode_t(child->type);
        memcpy(clone, child, sizeof(vnode_t));

        clone->real_node = child;
        clone->ref_count = 0;
        clone->next = nullptr;

        if (*ret == nullptr){
            *ret = clone;
            continue;
        }

        vnode_t *last = *ret;
        while (last->next) last = last->next;

        last->next = clone;
    }

    spin_unlock(child_lock, rflags);
    return cnt;
}

int vnode_t::mkdir(const char* name){
    if (this->type != VDIR) return -ENOTDIR;

    // It should not be called on a copy (Only call functions for files acquired via find_file).
    if (this->real_node) return this->real_node->mkdir(name);

    vnode_t* node = nullptr;
    int ret = this->find_file(name, &node);
    if (node) {
        node->close();
        return -EEXIST;
    }

    ret = -EOPNOTSUPP;

    if (this->is_mounted && this->mount_point->file_operations.mkdir){
        ret = this->mount_point->file_operations.mkdir((char*)name, this->mount_point);
    } else if (this->file_operations.mkdir){
        ret = this->file_operations.mkdir((char*)name, this);
    }

    if (this->directory_cached) this->should_cache_directory = true;
    this->directory_cached = false;

    return ret;
}

int vnode_t::creat(const char* name){
    if (this->type != VDIR) return -ENOTDIR;

    // It should not be called on a copy (Only call functions for files acquired via find_file).
    if (this->real_node) return this->real_node->creat(name);

    vnode_t* node = nullptr;
    int ret = this->find_file(name, &node);
    if (node) {
        node->close();
        ret = -EEXIST;
    } else if (this->file_operations.creat){
        ret = this->file_operations.creat((char*)name, this);
    }

    if (this->directory_cached) this->should_cache_directory = true;
    this->directory_cached = false;

    return ret;
}

void vnode_t::save_metadata(){
    if (this->file_operations.save_metadata){
        this->file_operations.save_metadata(this);
    }
}

int vnode_t::rmdir(){
    if (this->type != VDIR) return -ENOTDIR;
    
    // It should not be called on a copy (Only call functions for files acquired via find_file).
    if (this->real_node) return this->real_node->rmdir();

    if (this->file_operations.rmdir){
        int ret = this->file_operations.rmdir(this);
        if (ret == 0){
            if (this->parent) vnode_remove_from_child_list(this->parent, this);
            this->parent = nullptr;
        }

        if (this->parent && this->parent->directory_cached) {
            this->should_cache_directory = true;
            this->parent->directory_cached = false;
        }
        return ret;
    }
    

    return -EOPNOTSUPP;
}

int vnode_t::unlink(){
    // It should not be called on a copy (Only call functions for files acquired via find_file).
    if (this->real_node) return this->real_node->unlink();

    if (this->type == VDIR) return rmdir();

    if (this->file_operations.unlink){
        int ret = this->file_operations.unlink(this);
        if (ret == 0){
            if (this->parent) vnode_remove_from_child_list(this->parent, this);
            this->parent = nullptr;
        }

        if (this->parent && this->parent->directory_cached) {
            this->should_cache_directory = true;
            this->parent->directory_cached = false;
        }
        return ret;
    }

    return -EOPNOTSUPP;
}

char* procfs_read_link(vnode_t* node){
    task_t *task = task_scheduler::get_current_task();

    if (!task || !task->proc_vfs_dir){
        char *alloc = (char*)malloc(2);
        alloc[0] = '/';
        alloc[1] = '\0';
        return alloc;
    }

    return vfs::get_full_path_name(task->proc_vfs_dir);
}

int null_read(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    return 0;
}

int null_write(uint64_t offset, uint64_t length, const void* buffer, vnode_t* this_node){
    return length;
}

namespace vfs{
    vnode_t* root_node = nullptr;

    vnode_t* get_root_node(){
        return root_node;
    }

    void initialize_vfs(){
        root_node = filesystems::memfs::create_memfs();

        strcpy(root_node->name, "/");
        root_node->virtual_file = true;

        // Create the /dev subdirectory
        vnode_t* dev_dir = filesystems::memfs::create_memfs();
        strcpy(dev_dir->name, "dev");
        dev_dir->virtual_file = true;
        dev_dir->type = VDIR;

        vfs::add_node(vfs::get_root_node(), dev_dir);

        vnode_t *null = new vnode_t(VCHR);
        strcpy(null->name, "null");
        null->file_operations.read = null_read;
        null->file_operations.write = null_write;
        vfs::add_node(dev_dir, null);
        null->permissions = 0777;




        // Create /proc and /proc/bus
        vnode_t* root = vfs::get_root_node();
        root->mkdir("proc");
        
        vnode_t* proc = vfs::resolve_path("/proc");

        if (proc){
            proc->mkdir("bus");
            proc->close();

            proc->mkdir("sys");

            /* Add a dynamic link for the /proc/self directory */
            vnode_t *link = new vnode_t(VLNK);
            link->file_operations.read_link = procfs_read_link;
            strcpy(link->name, "self");
            add_node(proc, link);
        }

        vnode_t* sys = vfs::resolve_path("/proc/sys");
        if (sys){
            sys->mkdir("kernel");
            sys->close();
        }

        vnode_t* kernel = vfs::resolve_path("/proc/sys/kernel");

        if (kernel){
            kernel->creat("osrelease");
            kernel->close();
        }

        vnode_t* os_release = vfs::resolve_path("/proc/sys/kernel/osrelease");

        if (os_release){
            os_release->write(0, strlen(KERNEL_VERSION_STRING), KERNEL_VERSION_STRING);
            os_release->close();
        }

    }

    // @brief Will mount a fs node into node
    int mount(vnode_t* node, vnode_t* mount_target){
        if (mount_target->is_mounted) return -EEXIST;

        // Prepare to mount
        if (node->file_operations.mount != nullptr)
            node->file_operations.mount(node);

        node->open();
        // Mount
        mount_target->mount_point = node;
        mount_target->is_mounted = true;
        mount_target->virtual_file = true;
        
        node->is_mount_point = true;
        node->mounted_on = mount_target;

        return 0;
    }

    // @brief Will add a virtual inode to the parent (Exists only in memory)
    int add_node(vnode_t* parent, vnode_t* node){
        vnode_t* ret;

        int r = parent->find_file(node->name, &ret);
        if (r != -ENOENT) {
            node->close();
            return -EEXIST;
        }

        node->parent = parent;
        node->virtual_file = true;
        uint64_t rflags = spin_lock(&parent->child_list_lock);
        vnode_add_to_child_list(parent, node);
        spin_unlock(&parent->child_list_lock, rflags);
        node->open();
        return 0;
    }

    // @brief Will remove a node. Warning, make sure its ref_cnt is 0!
    int rm_node(vnode_t* parent, vnode_t* node){
        vnode_remove_from_child_list(parent, node);
        return 0;
    }

    // @brief Returns -EACC
    int vfs_check_permission(vnode_t* node, int desired_access) {
        task_t* current = task_scheduler::get_current_task();
        
        uint32_t subject_uid = current->euid;
        uint32_t subject_gid = current->egid;

        // Root check (Root can do anything)
        if (subject_uid == 0) return 0;

        // Owner check
        if (subject_uid == node->uid) {
            return (node->permissions & (desired_access << 6)) == (desired_access << 6) ? 0 : -EACCES;
        }

        // Group check
        if (subject_gid == node->gid) { // Also check supplementary groups list here!
            return (node->permissions & (desired_access << 3)) == (desired_access << 3) ? 0 : -EACCES;
        }

        // Other check
        return (node->permissions & desired_access) == desired_access ? 0 : -EACCES;
    }

    // Forward declaration for recursion
    static vnode_t* _resolve_path_recursion(vnode_t* start_node, char* path, int depth, bool follow_links, bool follow_trailing);

    // @warning The vnode returned is already open!
    vnode_t* resolve_path(const char* path, bool follow_links, bool follow_trailing) {
        if (!path) return nullptr;

        char* path_copy = strdup(path); 
        if (!path_copy) return nullptr;

        vnode_t* start_node = root_node;
        start_node->open();
        // Start the recursive walk
        vnode_t* result = _resolve_path_recursion(start_node, path_copy, 0, follow_links, follow_trailing);

        free(path_copy);
        return result;
    }

    static vnode_t* _resolve_path_recursion(vnode_t* start_node, char* path, int depth, bool follow_links, bool follow_trailing) {
        if (depth > MAX_SYMLINK_DEPTH) {
            start_node->close(); // Clean up where we were standing
            return nullptr; // -ELOOP
        }

        vnode_t* current = start_node; // We already hold a ref to this from the caller
        char* token;
        char* state; // For strtok_r context

        // 5. Tokenize the path
        token = strtok_r(path, PATH_SEPARATOR, &state);

        while (token != nullptr) {
            vnode_t* next = nullptr;

            // --- Handle ".." (Parent) ---
            if (strcmp(token, "..") == 0) {
                
                if (current->mounted_on) {
                    vnode_t* mount_dir = current->mounted_on;
                    if (mount_dir->parent) next = mount_dir->parent;
                    else next = mount_dir; // We are at root of host FS
                } else if (current->parent) {
                    // Standard parent
                    next = current->parent;
                } else {
                    // We are at root, ".." stays at root
                    next = current;
                }
                
                // Acquire next, Release current
                next->open();
                current->close();
                current = next;
                
                token = strtok_r(NULL, PATH_SEPARATOR, &state);
                continue;
            }

            if (strcmp(token, ".") == 0) {
                token = strtok_r(NULL, PATH_SEPARATOR, &state);
                continue;
            }

            if (current->find_file(token, &next) != 0) {
                return nullptr; // -ENOENT (Not found)
            }

            if (next->is_mounted && next->mount_point) {
                vnode_t* real_root = next->mount_point;
                real_root->open();
                next->close();
                next = real_root;
            }
            
            char* next_token_peek = strtok_r(NULL, PATH_SEPARATOR, &state); // Peek next
            bool is_last = (next_token_peek == nullptr);
            
            if ((next->type == VLNK) && follow_links && (!is_last || follow_trailing)) {
                
                // Read the link target
                char* link_path = next->read_link(); 
                if (link_path){
                    next->close();
                    
                    vnode_t* link_base;
                    if (link_path[0] == '/') {
                        link_base = root_node;

                        // Since we are jumping to root, we can release the directory we were in
                        current->close(); 
                    } else {
                        link_base = current; 
                    }
                    link_base->open();

                    // Recurse!
                    vnode_t* resolved_target = _resolve_path_recursion(link_base, link_path, depth + 1, true, true);
                    
                    free(link_path);

                    if (!resolved_target) return nullptr; // Broken link

                    // The resolved target becomes our 'next'
                    next = resolved_target;
                } else {
                    current->close();
                    return nullptr;
                }
            }

            
            current->close();
            current = next;   // Step into the child
            
            if (is_last) break; 
            
            token = next_token_peek;
        }

        return current;
    }

    void print_tree(vnode_t* node, int depth, int max_depth) {
        if (depth > max_depth) return;

        // 1. Indentation
        //    Print 2 spaces per depth level to show hierarchy
        for (int i = 0; i < depth; i++) {
            kprintf("  ");
        }

        // 2. Print Node Name and Type
        //    e.g., "|- home (DIR)"

        if (depth > 0) kprintf("|- ");
        kprintf("%s", node->name[0] ? node->name : "/"); // Handle root case

        if (node->type == VDIR) kprintf(" (DIR)");
        else if (node->type == VBLK) kprintf(" (BLK)");
        else if (node->type == VCHR) kprintf(" (CHR)");
        else if (node->type == VLNK) kprintf(" (LNK)");
        
        // Check if it's a mountpoint
        if (node->is_mounted) kprintf(" [MOUNT] (Usage: %d / %d)",
            node->mount_point->size / (1024 * 1024), node->mount_point->partition_total_size / (1024 * 1024));
        
        kprintf("\n");

        // 3. Recursion (If it's a directory)
        bool self_reference = memcmp(".", node->name, 2) == 0;
        bool parent_reference = memcmp("..", node->name, 3) == 0;
        if (node->type == VDIR && !self_reference && !parent_reference) {
            vnode_t* children_list = nullptr;
            
            // This creates a NEW linked list of node copies on the heap
            int count = node->read_dir(&children_list);

            vnode_t* iterator = children_list;
            while (iterator != nullptr) {
                // Save next pointer because we might delete iterator
                vnode_t* next_node = iterator->next;

                // Recurse down
                print_tree(iterator, depth + 1, max_depth);

                // CLEANUP: read_dir gave us a heap-allocated copy.
                // We must delete it now that we are done with it.
                delete iterator;

                iterator = next_node;
            }
        }
    }

    void recursive_path_builder(vnode_t* node, char* buffer, int& offset) {
        if (!node) return;

        if (node->mounted_on) {
            recursive_path_builder(node->mounted_on, buffer, offset);
            
            return; 
        }

        if (node->parent && node != vfs::root_node) {
            recursive_path_builder(node->parent, buffer, offset);
        }

        int len = strlen(node->name);
        
        if (offset > 0 && buffer[offset-1] != '/') {
            buffer[offset++] = '/';
        }
        
        strcpy(buffer + offset, node->name);
        offset += len;
    }

    char* get_full_path_name(vnode_t* node){
        char* buffer = (char*)malloc(2048);

        int offset = 0;
        recursive_path_builder(node, buffer, offset);

        return buffer;
    }

}

// @brief It will attempt to detect the filesystem and create an unmounted root node for it
vnode_t* create_root_node_fs(vnode_t* blk){
    if (filesystems::is_fat32(blk)){
        filesystems::fat32_t* fs = new filesystems::fat32_t(blk);
        return fs->create_root_node();
    }

    if (filesystems::is_ext2(blk)){
        filesystems::ext2_t* fs = new filesystems::ext2_t(blk);
        return fs->create_root_node();
    }
    
    return nullptr;
}